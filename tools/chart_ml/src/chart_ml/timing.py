from __future__ import annotations

from bisect import bisect_left
from dataclasses import dataclass
import math
import statistics

from chart_ml.audio_features import AudioAnalysis, strength_at_time
from chart_ml.models import Chart, TimingEvent
from chart_ml.timing_utils import tick_to_seconds

TIMING_MODE_FIXED = "fixed"
TIMING_MODE_ADAPTIVE = "adaptive"
ACTIVE_PRE_ROLL_SECONDS = 0.02
ACTIVE_POST_ROLL_SECONDS = 0.05


@dataclass(slots=True)
class TempoSegment:
    start_time_seconds: float
    end_time_seconds: float
    start_tick: int
    end_tick: int
    start_beat_index: int
    end_beat_index: int
    bpm: float


@dataclass(slots=True)
class TimingGeneration:
    resolution: int
    bpm: float
    meter_numerator: int
    meter_denominator: int
    event_times_seconds: list[float]
    event_ticks: list[int]
    timing_events: list[TimingEvent]
    subdivision: int
    timing_mode: str
    beat_times_seconds: list[float]
    beat_ticks: list[int]
    measure_start_ticks: list[int]
    tempo_segments: list[TempoSegment]
    ln_candidate_end_ticks: list[list[int]]


@dataclass(slots=True)
class TimingEvaluation:
    timing_mode: str
    generated_event_count: int
    generated_beat_count: int
    chart_beat_count: int
    note_count: int
    note_mean_abs_error_ms: float
    note_p90_abs_error_ms: float
    note_hit_rate_30ms: float
    note_hit_rate_60ms: float
    note_hit_rate_90ms: float
    beat_mean_abs_error_ms: float
    beat_p90_abs_error_ms: float
    beat_hit_rate_30ms: float
    beat_hit_rate_60ms: float
    tempo_segment_count: int
    bpm_min: float
    bpm_max: float
    bpm_median: float


def choose_subdivision(level: float) -> int:
    if level < 3.0:
        return 2
    if level < 5.0:
        return 4
    if level < 7.0:
        return 6
    if level < 9.0:
        return 8
    if level < 11.0:
        return 12
    return 16


def _ticks_per_second(bpm: float, resolution: int) -> float:
    return (bpm / 60.0) * resolution


def _quantize_bpm(value: float, step: float = 0.25) -> float:
    if value <= 0:
        return 120.0
    return round(value / step) * step


def _base_strength_ratio(level: float) -> float:
    if level < 4.0:
        return 0.30
    if level < 6.0:
        return 0.22
    if level < 8.0:
        return 0.16
    if level < 10.0:
        return 0.12
    return 0.08


def _percentile(values: list[float], ratio: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((len(ordered) - 1) * ratio))))
    return float(ordered[index])


def _nearest_delta(values: list[float], target: float) -> float:
    if not values:
        return math.inf
    index = bisect_left(values, target)
    candidates: list[float] = []
    if index < len(values):
        candidates.append(abs(values[index] - target))
    if index > 0:
        candidates.append(abs(values[index - 1] - target))
    return min(candidates) if candidates else math.inf


def _estimate_reference_bpm(analysis: AudioAnalysis, bpm_override: float | None = None) -> float:
    if bpm_override is not None and bpm_override > 0:
        return float(bpm_override)
    base_bpm = float(analysis.tempo_bpm or 120.0)
    if len(analysis.beat_times) >= 2:
        intervals = [analysis.beat_times[index + 1] - analysis.beat_times[index] for index in range(len(analysis.beat_times) - 1)]
        positive = [interval for interval in intervals if interval > 1e-4]
        if positive:
            base_bpm = 60.0 / statistics.median(positive)
    return _select_bpm_hypothesis(analysis, base_bpm)


def _active_window(analysis: AudioAnalysis) -> tuple[float, float]:
    start_time = max(0.0, analysis.active_start_seconds - ACTIVE_PRE_ROLL_SECONDS)
    end_time = min(analysis.duration_seconds, analysis.active_end_seconds + ACTIVE_POST_ROLL_SECONDS)
    if end_time <= start_time:
        return 0.0, analysis.duration_seconds
    return start_time, end_time


def _significant_onset_samples(analysis: AudioAnalysis) -> list[tuple[float, float]]:
    if not analysis.onset_times or not analysis.onset_strength_values:
        return []
    active_start, active_end = _active_window(analysis)
    max_strength = max(analysis.onset_strength_values) if analysis.onset_strength_values else 0.0
    threshold = max_strength * 0.35
    samples: list[tuple[float, float]] = []
    for onset_time in analysis.onset_times:
        if onset_time < active_start or onset_time > active_end:
            continue
        strength = strength_at_time(analysis, onset_time)
        if strength >= threshold:
            samples.append((onset_time, strength))
    return samples


def _candidate_bpm_values(base_bpm: float) -> list[float]:
    candidates = {
        float(base_bpm),
        _quantize_bpm(base_bpm, step=0.25),
        _quantize_bpm(base_bpm, step=0.5),
        float(round(base_bpm)),
        float(math.floor(base_bpm)),
        float(math.ceil(base_bpm)),
        float(round(base_bpm - 1.0)),
        float(round(base_bpm + 1.0)),
    }
    ordered = sorted(candidate for candidate in candidates if 60.0 <= candidate <= 240.0)
    return ordered or [float(base_bpm)]


def _score_bpm_candidate(analysis: AudioAnalysis, bpm: float) -> float:
    onset_samples = _significant_onset_samples(analysis)
    if not onset_samples:
        return -abs(bpm - float(analysis.tempo_bpm or bpm))

    beat_duration = 60.0 / max(bpm, 1e-6)
    max_strength = max(strength for _, strength in onset_samples)
    anchor_samples = onset_samples[: min(8, len(onset_samples))]
    best_score = -math.inf

    for anchor_time, _ in anchor_samples:
        anchor_phase = anchor_time % beat_duration
        score = 0.0
        for onset_time, strength in onset_samples:
            relative = abs(((onset_time - anchor_phase) / beat_duration) - round((onset_time - anchor_phase) / beat_duration))
            beat_alignment = 1.0 - min(1.0, relative * 4.0)
            score += (strength / max(max_strength, 1e-6)) * beat_alignment
        duration_penalty = abs(bpm - float(analysis.tempo_bpm or bpm)) * 0.025
        score -= duration_penalty
        if score > best_score:
            best_score = score
    return best_score


def _select_bpm_hypothesis(analysis: AudioAnalysis, base_bpm: float) -> float:
    best_bpm = float(base_bpm)
    best_score = -math.inf
    for candidate in _candidate_bpm_values(base_bpm):
        score = _score_bpm_candidate(analysis, candidate)
        if score > best_score:
            best_score = score
            best_bpm = candidate
    return best_bpm


def _build_fixed_beat_times(analysis: AudioAnalysis, bpm: float) -> list[float]:
    beat_duration = 60.0 / max(bpm, 1e-6)
    beat_times: list[float] = []
    current_time = 0.0
    while current_time <= analysis.duration_seconds + beat_duration + 1e-6:
        beat_times.append(current_time)
        current_time += beat_duration
    if not beat_times:
        beat_times = [0.0, beat_duration]
    elif len(beat_times) == 1:
        beat_times.append(beat_times[0] + beat_duration)
    return beat_times


def _normalize_beat_times(analysis: AudioAnalysis, bpm_override: float | None = None) -> list[float]:
    bpm = _estimate_reference_bpm(analysis, bpm_override=bpm_override)
    fallback_duration = 60.0 / max(bpm, 1e-6)
    if len(analysis.beat_times) < 2:
        return _build_fixed_beat_times(analysis, bpm)

    beat_times = [float(value) for value in sorted(analysis.beat_times) if 0.0 <= float(value) <= analysis.duration_seconds + 1.0]
    if len(beat_times) < 2:
        return _build_fixed_beat_times(analysis, bpm)

    intervals = [beat_times[index + 1] - beat_times[index] for index in range(len(beat_times) - 1)]
    positive_intervals = [interval for interval in intervals if interval > 1e-4]
    base_interval = statistics.median(positive_intervals) if positive_intervals else fallback_duration

    while beat_times and beat_times[0] > base_interval * 0.65:
        beat_times.insert(0, beat_times[0] - base_interval)
    if not beat_times or beat_times[0] > 1e-6:
        beat_times.insert(0, 0.0)
    else:
        beat_times[0] = 0.0

    tail_interval = statistics.median(positive_intervals[-4:]) if positive_intervals else base_interval
    while beat_times[-1] < analysis.duration_seconds + 1e-6:
        beat_times.append(beat_times[-1] + tail_interval)
    if len(beat_times) < 2:
        beat_times.append(beat_times[0] + base_interval)
    return beat_times


def _local_bpms_from_beats(beat_times_seconds: list[float], fallback_bpm: float) -> list[float]:
    bpms: list[float] = []
    for index in range(max(0, len(beat_times_seconds) - 1)):
        delta = beat_times_seconds[index + 1] - beat_times_seconds[index]
        if delta <= 1e-4:
            bpms.append(float(fallback_bpm))
        else:
            bpms.append(60.0 / delta)
    if not bpms:
        bpms.append(float(fallback_bpm))
    return bpms


def _relative_bpm_variation(bpms: list[float]) -> float:
    if len(bpms) < 2:
        return 0.0
    median_bpm = statistics.median(bpms)
    if median_bpm <= 1e-6:
        return 0.0
    return statistics.pstdev(bpms) / median_bpm


def _smooth_bpms(bpms: list[float], window_radius: int = 2) -> list[float]:
    if not bpms:
        return []
    smoothed: list[float] = []
    for index in range(len(bpms)):
        start = max(0, index - window_radius)
        end = min(len(bpms), index + window_radius + 1)
        smoothed.append(float(statistics.median(bpms[start:end])))
    return smoothed


def _build_tempo_segments(
    beat_times_seconds: list[float],
    beat_ticks: list[int],
    fallback_bpm: float,
    min_segment_beats: int = 8,
    change_threshold_ratio: float = 0.12,
) -> list[TempoSegment]:
    local_bpms = _smooth_bpms(_local_bpms_from_beats(beat_times_seconds, fallback_bpm), window_radius=2)
    if not local_bpms:
        return [
            TempoSegment(
                start_time_seconds=0.0,
                end_time_seconds=max(0.0, beat_times_seconds[-1] if beat_times_seconds else 0.0),
                start_tick=0,
                end_tick=beat_ticks[-1] if beat_ticks else 0,
                start_beat_index=0,
                end_beat_index=max(1, len(beat_times_seconds) - 1),
                bpm=fallback_bpm,
            )
        ]

    segments: list[TempoSegment] = []
    segment_start = 0
    for index in range(1, len(local_bpms)):
        segment_values = local_bpms[segment_start:index]
        reference_bpm = statistics.median(segment_values) if segment_values else local_bpms[segment_start]
        ratio = abs(local_bpms[index] - reference_bpm) / max(reference_bpm, 1e-6)
        quantized_reference = _quantize_bpm(reference_bpm)
        quantized_current = _quantize_bpm(local_bpms[index])
        enough_length = (index - segment_start) >= min_segment_beats
        if ratio >= change_threshold_ratio and enough_length and quantized_reference != quantized_current:
            segment_bpm = _quantize_bpm(reference_bpm)
            segments.append(
                TempoSegment(
                    start_time_seconds=beat_times_seconds[segment_start],
                    end_time_seconds=beat_times_seconds[index],
                    start_tick=beat_ticks[segment_start],
                    end_tick=beat_ticks[index],
                    start_beat_index=segment_start,
                    end_beat_index=index,
                    bpm=segment_bpm,
                )
            )
            segment_start = index

    segment_bpm = _quantize_bpm(statistics.median(local_bpms[segment_start:]))
    segments.append(
        TempoSegment(
            start_time_seconds=beat_times_seconds[segment_start],
            end_time_seconds=beat_times_seconds[-1],
            start_tick=beat_ticks[segment_start],
            end_tick=beat_ticks[-1],
            start_beat_index=segment_start,
            end_beat_index=len(beat_times_seconds) - 1,
            bpm=segment_bpm,
        )
    )
    merged_segments: list[TempoSegment] = []
    for segment in segments:
        if merged_segments and math.isclose(merged_segments[-1].bpm, segment.bpm, abs_tol=1e-6):
            previous = merged_segments[-1]
            merged_segments[-1] = TempoSegment(
                start_time_seconds=previous.start_time_seconds,
                end_time_seconds=segment.end_time_seconds,
                start_tick=previous.start_tick,
                end_tick=segment.end_tick,
                start_beat_index=previous.start_beat_index,
                end_beat_index=segment.end_beat_index,
                bpm=previous.bpm,
            )
        else:
            merged_segments.append(segment)
    if merged_segments and merged_segments[0].start_tick != 0:
        first = merged_segments[0]
        merged_segments[0] = TempoSegment(
            start_time_seconds=0.0,
            end_time_seconds=first.end_time_seconds,
            start_tick=0,
            end_tick=first.end_tick,
            start_beat_index=0,
            end_beat_index=first.end_beat_index,
            bpm=first.bpm,
        )
    return merged_segments


def _time_to_tick_from_beats(time_seconds: float, beat_times_seconds: list[float], resolution: int) -> int:
    if not beat_times_seconds:
        return 0
    if len(beat_times_seconds) == 1:
        return 0
    clamped = min(max(time_seconds, 0.0), beat_times_seconds[-1])
    index = bisect_left(beat_times_seconds, clamped)
    if index <= 0:
        left_index = 0
        right_index = 1
    elif index >= len(beat_times_seconds):
        left_index = len(beat_times_seconds) - 2
        right_index = len(beat_times_seconds) - 1
    elif beat_times_seconds[index] == clamped:
        return index * resolution
    else:
        left_index = index - 1
        right_index = index
    start_time = beat_times_seconds[left_index]
    end_time = beat_times_seconds[right_index]
    if end_time <= start_time:
        beat_position = float(left_index)
    else:
        beat_position = left_index + ((clamped - start_time) / (end_time - start_time))
    return int(round(beat_position * resolution))


def _generate_events_from_beats(
    analysis: AudioAnalysis,
    beat_times_seconds: list[float],
    resolution: int,
    subdivision: int,
    min_strength_ratio: float,
) -> tuple[list[float], list[int]]:
    active_start, active_end = _active_window(analysis)
    max_strength = max(analysis.onset_strength_values) if analysis.onset_strength_values else 0.0
    threshold = max_strength * min_strength_ratio
    event_times: list[float] = []
    event_ticks: list[int] = []
    last_tick = -1

    for beat_index in range(max(0, len(beat_times_seconds) - 1)):
        beat_start = beat_times_seconds[beat_index]
        beat_end = beat_times_seconds[beat_index + 1]
        beat_duration = max(beat_end - beat_start, 1e-4)
        for subdivision_index in range(subdivision):
            time_seconds = beat_start + (beat_duration * subdivision_index / subdivision)
            if time_seconds < active_start - 1e-6:
                continue
            if time_seconds > active_end + 1e-6:
                continue
            position_ratio = subdivision_index / max(subdivision - 1, 1)
            is_beat = subdivision_index == 0
            is_half = subdivision_index * 2 == subdivision
            strength = strength_at_time(analysis, time_seconds)
            local_threshold = threshold
            if is_beat:
                local_threshold *= 0.0
            elif is_half:
                local_threshold *= 0.75
            else:
                local_threshold *= 0.9 + position_ratio * 0.1
            if strength < local_threshold and not is_beat:
                continue
            tick = _time_to_tick_from_beats(time_seconds, beat_times_seconds, resolution)
            if tick <= last_tick:
                continue
            event_times.append(time_seconds)
            event_ticks.append(tick)
            last_tick = tick

    if not event_ticks:
        event_times = [0.0]
        event_ticks = [0]
    return event_times, event_ticks


def _build_ln_candidate_end_ticks(event_ticks: list[int], resolution: int) -> list[list[int]]:
    candidates_by_event: list[list[int]] = []
    if not event_ticks:
        return candidates_by_event

    for tick in event_ticks:
        candidate_targets = [tick + resolution // 2, tick + resolution, tick + resolution * 2, tick + resolution * 4]
        lane_candidates: list[int] = []
        for target in candidate_targets:
            index = bisect_left(event_ticks, target)
            if index >= len(event_ticks):
                continue
            candidate_tick = event_ticks[index]
            if candidate_tick > tick and candidate_tick not in lane_candidates:
                lane_candidates.append(candidate_tick)
        candidates_by_event.append(lane_candidates)
    return candidates_by_event


def generate_fixed_bpm_timing(
    analysis: AudioAnalysis,
    level: float,
    resolution: int = 480,
    meter_numerator: int = 4,
    meter_denominator: int = 4,
    bpm_override: float | None = None,
    min_strength_ratio: float | None = None,
) -> TimingGeneration:
    bpm = float(_estimate_reference_bpm(analysis, bpm_override=bpm_override))
    subdivision = choose_subdivision(level)
    beat_times_seconds = _build_fixed_beat_times(analysis, bpm)
    beat_ticks = [index * resolution for index in range(len(beat_times_seconds))]

    if min_strength_ratio is None:
        min_strength_ratio = _base_strength_ratio(level)

    event_times, event_ticks = _generate_events_from_beats(
        analysis=analysis,
        beat_times_seconds=beat_times_seconds,
        resolution=resolution,
        subdivision=subdivision,
        min_strength_ratio=min_strength_ratio,
    )
    timing_events = [
        TimingEvent(event_type="meter", tick=0, numerator=meter_numerator, denominator=meter_denominator),
        TimingEvent(event_type="bpm", tick=0, bpm=bpm),
    ]
    measure_start_ticks = list(range(0, beat_ticks[-1] + 1, meter_numerator * resolution)) if beat_ticks else [0]
    return TimingGeneration(
        resolution=resolution,
        bpm=bpm,
        meter_numerator=meter_numerator,
        meter_denominator=meter_denominator,
        event_times_seconds=event_times,
        event_ticks=event_ticks,
        timing_events=timing_events,
        subdivision=subdivision,
        timing_mode=TIMING_MODE_FIXED,
        beat_times_seconds=beat_times_seconds,
        beat_ticks=beat_ticks,
        measure_start_ticks=measure_start_ticks,
        tempo_segments=[
            TempoSegment(
                start_time_seconds=0.0,
                end_time_seconds=beat_times_seconds[-1],
                start_tick=0,
                end_tick=beat_ticks[-1],
                start_beat_index=0,
                end_beat_index=len(beat_times_seconds) - 1,
                bpm=bpm,
            )
        ],
        ln_candidate_end_ticks=_build_ln_candidate_end_ticks(event_ticks, resolution),
    )


def generate_adaptive_timing(
    analysis: AudioAnalysis,
    level: float,
    resolution: int = 480,
    meter_numerator: int = 4,
    meter_denominator: int = 4,
    bpm_override: float | None = None,
    min_strength_ratio: float | None = None,
    min_segment_beats: int = 8,
    change_threshold_ratio: float = 0.12,
) -> TimingGeneration:
    reference_bpm = _estimate_reference_bpm(analysis, bpm_override=bpm_override)
    subdivision = choose_subdivision(level)
    beat_times_seconds = _normalize_beat_times(analysis, bpm_override=bpm_override)
    local_bpms = _local_bpms_from_beats(beat_times_seconds, reference_bpm)
    variation = _relative_bpm_variation(local_bpms)
    if variation <= 0.03 or variation >= 0.15:
        beat_times_seconds = _build_fixed_beat_times(analysis, reference_bpm)
    beat_ticks = [index * resolution for index in range(len(beat_times_seconds))]

    if min_strength_ratio is None:
        min_strength_ratio = _base_strength_ratio(level)

    event_times, event_ticks = _generate_events_from_beats(
        analysis=analysis,
        beat_times_seconds=beat_times_seconds,
        resolution=resolution,
        subdivision=subdivision,
        min_strength_ratio=min_strength_ratio,
    )
    tempo_segments = _build_tempo_segments(
        beat_times_seconds=beat_times_seconds,
        beat_ticks=beat_ticks,
        fallback_bpm=reference_bpm,
        min_segment_beats=min_segment_beats,
        change_threshold_ratio=change_threshold_ratio,
    )
    timing_events = [TimingEvent(event_type="meter", tick=0, numerator=meter_numerator, denominator=meter_denominator)]
    for segment in tempo_segments:
        timing_events.append(TimingEvent(event_type="bpm", tick=segment.start_tick, bpm=segment.bpm))
    timing_events.sort(key=lambda event: (event.tick, 0 if event.event_type == "meter" else 1))

    measure_start_ticks = list(range(0, beat_ticks[-1] + 1, meter_numerator * resolution)) if beat_ticks else [0]
    bpm_values = [segment.bpm for segment in tempo_segments] or [reference_bpm]
    return TimingGeneration(
        resolution=resolution,
        bpm=float(statistics.median(bpm_values)),
        meter_numerator=meter_numerator,
        meter_denominator=meter_denominator,
        event_times_seconds=event_times,
        event_ticks=event_ticks,
        timing_events=timing_events,
        subdivision=subdivision,
        timing_mode=TIMING_MODE_ADAPTIVE,
        beat_times_seconds=beat_times_seconds,
        beat_ticks=beat_ticks,
        measure_start_ticks=measure_start_ticks,
        tempo_segments=tempo_segments,
        ln_candidate_end_ticks=_build_ln_candidate_end_ticks(event_ticks, resolution),
    )


def generate_timing(
    analysis: AudioAnalysis,
    level: float,
    resolution: int = 480,
    meter_numerator: int = 4,
    meter_denominator: int = 4,
    bpm_override: float | None = None,
    min_strength_ratio: float | None = None,
    timing_mode: str = TIMING_MODE_ADAPTIVE,
) -> TimingGeneration:
    if timing_mode == TIMING_MODE_FIXED:
        return generate_fixed_bpm_timing(
            analysis=analysis,
            level=level,
            resolution=resolution,
            meter_numerator=meter_numerator,
            meter_denominator=meter_denominator,
            bpm_override=bpm_override,
            min_strength_ratio=min_strength_ratio,
        )
    if timing_mode == TIMING_MODE_ADAPTIVE:
        return generate_adaptive_timing(
            analysis=analysis,
            level=level,
            resolution=resolution,
            meter_numerator=meter_numerator,
            meter_denominator=meter_denominator,
            bpm_override=bpm_override,
            min_strength_ratio=min_strength_ratio,
        )
    raise ValueError(f"Unknown timing mode: {timing_mode}")


def _chart_beat_times(chart: Chart) -> list[float]:
    last_tick = max([note.end_tick or note.tick for note in chart.notes], default=0)
    beat_ticks = list(range(0, last_tick + chart.meta.resolution, chart.meta.resolution))
    return [tick_to_seconds(chart, tick) for tick in beat_ticks]


def evaluate_timing_against_chart(generated: TimingGeneration, chart: Chart) -> TimingEvaluation:
    note_times_seconds = [tick_to_seconds(chart, note.tick) for note in chart.notes]
    note_deltas_ms = [(_nearest_delta(generated.event_times_seconds, note_time) * 1000.0) for note_time in note_times_seconds]

    chart_beat_times = _chart_beat_times(chart)
    beat_deltas_ms = [(_nearest_delta(generated.beat_times_seconds, beat_time) * 1000.0) for beat_time in chart_beat_times]
    bpm_values = [segment.bpm for segment in generated.tempo_segments] or [generated.bpm]

    def _hit_rate(values: list[float], threshold_ms: float) -> float:
        if not values:
            return 0.0
        hits = sum(1 for value in values if value <= threshold_ms)
        return hits / len(values)

    return TimingEvaluation(
        timing_mode=generated.timing_mode,
        generated_event_count=len(generated.event_ticks),
        generated_beat_count=len(generated.beat_ticks),
        chart_beat_count=len(chart_beat_times),
        note_count=len(note_times_seconds),
        note_mean_abs_error_ms=(sum(note_deltas_ms) / len(note_deltas_ms)) if note_deltas_ms else 0.0,
        note_p90_abs_error_ms=_percentile(note_deltas_ms, 0.9),
        note_hit_rate_30ms=_hit_rate(note_deltas_ms, 30.0),
        note_hit_rate_60ms=_hit_rate(note_deltas_ms, 60.0),
        note_hit_rate_90ms=_hit_rate(note_deltas_ms, 90.0),
        beat_mean_abs_error_ms=(sum(beat_deltas_ms) / len(beat_deltas_ms)) if beat_deltas_ms else 0.0,
        beat_p90_abs_error_ms=_percentile(beat_deltas_ms, 0.9),
        beat_hit_rate_30ms=_hit_rate(beat_deltas_ms, 30.0),
        beat_hit_rate_60ms=_hit_rate(beat_deltas_ms, 60.0),
        tempo_segment_count=len(generated.tempo_segments),
        bpm_min=min(bpm_values),
        bpm_max=max(bpm_values),
        bpm_median=float(statistics.median(bpm_values)),
    )
