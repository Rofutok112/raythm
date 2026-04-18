from __future__ import annotations

import json
import random
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

from chart_ml.corpus import load_manifest_selection
from chart_ml.models import Chart, ChartMeta, Note, TimingEvent
from chart_ml.rchart import discover_song_packages, parse_chart, write_chart

Pattern = str
LANE_STATE_ORDER = ("off", "tap", "hold_start", "holding", "hold_end")


def _pattern_key(lane_states: list[str]) -> Pattern:
    return "|".join(lane_states)


def _pattern_states(pattern: Pattern) -> list[str]:
    return pattern.split("|")


def _event_ticks(chart: Chart) -> list[int]:
    ticks = {event.tick for event in chart.timing_events if event.event_type != "meter"}
    for note in chart.notes:
        ticks.add(note.tick)
        if note.note_type == "hold" and note.end_tick is not None:
            ticks.add(note.end_tick)
    return sorted(ticks)


def _lane_state_at_tick(chart: Chart, tick: int, lane: int) -> str:
    state = "off"
    for note in chart.notes:
        if note.lane != lane:
            continue
        if note.note_type == "tap" and note.tick == tick:
            return "tap"
        if note.note_type == "hold":
            end_tick = note.end_tick if note.end_tick is not None else note.tick
            if note.tick == tick:
                return "hold_start"
            if end_tick == tick:
                state = "hold_end"
            elif note.tick < tick < end_tick:
                state = "holding"
    return state


def _chart_patterns(chart: Chart) -> list[Pattern]:
    patterns: list[Pattern] = []
    for tick in _event_ticks(chart):
        lane_states = [_lane_state_at_tick(chart, tick, lane) for lane in range(chart.meta.key_count)]
        patterns.append(_pattern_key(lane_states))
    return patterns


def _weighted_choice(counter: Counter[str], rng: random.Random) -> str:
    total = sum(counter.values())
    if total <= 0:
        raise ValueError("Cannot sample from empty counter")
    target = rng.uniform(0, total)
    cumulative = 0.0
    for key, weight in counter.items():
        cumulative += weight
        if cumulative >= target:
            return key
    return next(iter(counter))


def _is_valid_transition(previous: Pattern, current: Pattern) -> bool:
    previous_states = _pattern_states(previous)
    current_states = _pattern_states(current)
    if len(previous_states) != len(current_states):
        return False

    for previous_state, current_state in zip(previous_states, current_states):
        if previous_state in {"off", "tap", "hold_end"} and current_state == "holding":
            return False
        if current_state not in LANE_STATE_ORDER:
            return False
        if previous_state in {"hold_start", "holding"} and current_state not in {"holding", "hold_end"}:
            return False
        if previous_state in {"off", "tap", "hold_end"} and current_state == "hold_end":
            return False
    return True


def _pattern_density(pattern: Pattern) -> float:
    states = _pattern_states(pattern)
    active = 0.0
    for state in states:
        if state == "tap":
            active += 1.0
        elif state == "hold_start":
            active += 1.2
        elif state == "holding":
            active += 0.5
        elif state == "hold_end":
            active += 0.3
    return active / max(len(states), 1)


@dataclass(slots=True)
class SimplePatternModel:
    resolution: int
    subdivision: int
    initial_counts: Counter[str]
    transition_counts: dict[str, Counter[str]]
    beat_position_counts: dict[int, Counter[str]]
    average_events_per_beat: float

    def to_dict(self) -> dict[str, object]:
        return {
            "resolution": self.resolution,
            "subdivision": self.subdivision,
            "initial_counts": dict(self.initial_counts),
            "transition_counts": {key: dict(value) for key, value in self.transition_counts.items()},
            "beat_position_counts": {str(key): dict(value) for key, value in self.beat_position_counts.items()},
            "average_events_per_beat": self.average_events_per_beat,
        }

    @classmethod
    def from_dict(cls, payload: dict[str, object]) -> "SimplePatternModel":
        return cls(
            resolution=int(payload["resolution"]),
            subdivision=int(payload["subdivision"]),
            initial_counts=Counter(payload["initial_counts"]),
            transition_counts={key: Counter(value) for key, value in dict(payload["transition_counts"]).items()},
            beat_position_counts={int(key): Counter(value) for key, value in dict(payload["beat_position_counts"]).items()},
            average_events_per_beat=float(payload["average_events_per_beat"]),
        )

    def save(self, path: str | Path) -> None:
        target = Path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(self.to_dict(), ensure_ascii=True, indent=2) + "\n", encoding="utf-8", newline="\n")

    @classmethod
    def load(cls, path: str | Path) -> "SimplePatternModel":
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
        return cls.from_dict(payload)


def train_simple_model(
    songs_dir: str | Path,
    key_count: int = 4,
    subdivision: int = 4,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> SimplePatternModel:
    initial_counts: Counter[str] = Counter()
    transition_counts: dict[str, Counter[str]] = defaultdict(Counter)
    beat_position_counts: dict[int, Counter[str]] = defaultdict(Counter)
    total_events = 0
    total_beats = 0.0
    resolution_reference = 480
    selection = load_manifest_selection(manifest_path, split=split, included_only=True) if manifest_path is not None else None

    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            if chart.meta.key_count != key_count:
                continue
            if selection is not None and (song.meta.song_id, chart.meta.chart_id) not in selection:
                continue
            resolution_reference = chart.meta.resolution
            patterns = _chart_patterns(chart)
            ticks = _event_ticks(chart)
            if not patterns or not ticks:
                continue

            initial_counts[patterns[0]] += 1
            total_events += len(patterns)
            total_beats += ticks[-1] / max(chart.meta.resolution, 1)

            for index, pattern in enumerate(patterns):
                beat_position = int(round((ticks[index] / max(chart.meta.resolution, 1)) * subdivision)) % subdivision
                beat_position_counts[beat_position][pattern] += 1
                if index > 0:
                    transition_counts[patterns[index - 1]][pattern] += 1

    average_events_per_beat = total_events / total_beats if total_beats > 0.0 else 1.0
    return SimplePatternModel(
        resolution=resolution_reference,
        subdivision=subdivision,
        initial_counts=initial_counts,
        transition_counts=dict(transition_counts),
        beat_position_counts=dict(beat_position_counts),
        average_events_per_beat=average_events_per_beat,
    )


def _generate_patterns_for_ticks(model: SimplePatternModel, ticks: list[int], rng: random.Random) -> list[Pattern]:
    if not ticks:
        return []

    patterns: list[Pattern] = []
    current = _weighted_choice(model.initial_counts, rng)
    patterns.append(current)

    while len(patterns) < len(ticks):
        current_tick = ticks[len(patterns)]
        beat_position = int(round((current_tick / max(model.resolution, 1)) * model.subdivision)) % model.subdivision
        transition_counter = model.transition_counts.get(current, Counter())
        position_counter = model.beat_position_counts.get(beat_position, Counter())
        merged = Counter()
        merged.update(position_counter)
        merged.update({key: value * 2 for key, value in transition_counter.items()})
        if not merged:
            merged = model.initial_counts.copy()

        candidates = Counter({pattern: weight for pattern, weight in merged.items() if _is_valid_transition(current, pattern)})
        if not candidates:
            candidates = model.initial_counts.copy()

        current = _weighted_choice(candidates, rng)
        patterns.append(current)
    return patterns


def _generate_patterns_for_audio_times(
    model: SimplePatternModel,
    ticks: list[int],
    times_seconds: list[float],
    strengths: list[float],
    level: float,
    rng: random.Random,
) -> list[Pattern]:
    if not ticks:
        return []
    if len(ticks) != len(times_seconds) or len(ticks) != len(strengths):
        raise ValueError("ticks, times_seconds, and strengths must have the same length")

    max_strength = max(strengths) if strengths else 0.0
    normalized = [value / max_strength if max_strength > 0.0 else 0.0 for value in strengths]
    difficulty_scale = max(0.0, min(1.0, (level - 4.0) / 8.0))

    patterns: list[Pattern] = []
    current = _weighted_choice(model.initial_counts, rng)
    patterns.append(current)

    for index in range(1, len(ticks)):
        current_tick = ticks[index]
        beat_position = int(round((current_tick / max(model.resolution, 1)) * model.subdivision)) % model.subdivision
        strength = normalized[index]

        transition_counter = model.transition_counts.get(current, Counter())
        position_counter = model.beat_position_counts.get(beat_position, Counter())
        merged = Counter()
        merged.update(position_counter)
        merged.update({key: value * 2 for key, value in transition_counter.items()})
        if not merged:
            merged = model.initial_counts.copy()

        candidates = Counter()
        target_density = min(1.0, max(0.0, 0.10 + difficulty_scale * 0.28 + strength * (1.0 + difficulty_scale * 0.75)))
        for pattern, weight in merged.items():
            if not _is_valid_transition(current, pattern):
                continue
            density = _pattern_density(pattern)
            density_bias = max(0.05, 1.0 - abs(density - target_density))
            if strength < 0.18 and density > 0.6:
                density_bias *= max(0.18, 0.55 - difficulty_scale * 0.35)
            if strength > 0.7 and density < 0.2 + difficulty_scale * 0.1:
                density_bias *= max(0.2, 0.45 - difficulty_scale * 0.15)
            if difficulty_scale > 0.45 and density < 0.35:
                density_bias *= 0.55
            if difficulty_scale > 0.7 and density >= 0.7:
                density_bias *= 1.25
            candidates[pattern] += weight * density_bias

        if not candidates:
            candidates = Counter({pattern: weight for pattern, weight in merged.items() if _is_valid_transition(current, pattern)})
        if not candidates:
            candidates = model.initial_counts.copy()

        current = _weighted_choice(candidates, rng)
        patterns.append(current)

    return patterns


def _patterns_to_chart(patterns: list[Pattern],
                       ticks: list[int],
                       timing_events: list[TimingEvent],
                       chart_id: str,
                       song_id: str,
                       difficulty: str,
                       chart_author: str,
                       resolution: int) -> Chart:
    notes: list[Note] = []
    active_holds: dict[int, int] = {}

    for step_index, pattern in enumerate(patterns):
        tick = ticks[step_index]
        states = _pattern_states(pattern)
        for lane, state in enumerate(states):
            if lane in active_holds:
                if state == "hold_end":
                    start_tick = active_holds.pop(lane, None)
                    if start_tick is not None and tick > start_tick:
                        notes.append(Note(note_type="hold", tick=start_tick, lane=lane, end_tick=tick))
                continue

            if state == "tap":
                notes.append(Note(note_type="tap", tick=tick, lane=lane))
            elif state == "hold_start":
                active_holds[lane] = tick

    final_tick = ticks[-1] if ticks else 0
    for lane, start_tick in list(active_holds.items()):
        end_tick = final_tick
        if end_tick > start_tick:
            notes.append(Note(note_type="hold", tick=start_tick, lane=lane, end_tick=end_tick))

    notes.sort(key=lambda note: (note.tick, note.lane, note.end_tick or note.tick))
    return Chart(
        path="",
        meta=ChartMeta(
            chart_id=chart_id,
            key_count=4,
            difficulty=difficulty,
            level=5.0,
            chart_author=chart_author,
            format_version=1,
            resolution=resolution,
            offset=0,
            song_id=song_id,
            is_public=False,
        ),
        timing_events=list(timing_events),
        notes=notes,
    )


def generate_chart_from_template(model: SimplePatternModel,
                                 template_chart_path: str | Path,
                                 output_path: str | Path,
                                 song_id: str,
                                 chart_id: str,
                                 difficulty: str,
                                 seed: int = 0,
                                 chart_author: str = "chart_ml simple model") -> Chart:
    rng = random.Random(seed)
    template_chart = parse_chart(template_chart_path)
    ticks = _event_ticks(template_chart)
    if not ticks:
        raise ValueError(f"Template chart has no event ticks: {template_chart_path}")

    patterns = _generate_patterns_for_ticks(model, ticks=ticks, rng=rng)
    chart = _patterns_to_chart(
        patterns=patterns,
        ticks=ticks,
        timing_events=template_chart.timing_events,
        chart_id=chart_id,
        song_id=song_id,
        difficulty=difficulty,
        chart_author=chart_author,
        resolution=model.resolution,
    )
    write_chart(chart, output_path)
    return chart


def generate_chart_from_audio(
    model: SimplePatternModel,
    audio_path: str | Path,
    output_path: str | Path,
    song_id: str,
    chart_id: str,
    difficulty: str,
    level: float,
    bpm_override: float | None = None,
    timing_mode: str = "adaptive",
    seed: int = 0,
    chart_author: str = "chart_ml audio model",
) -> Chart:
    from chart_ml.audio_features import analyze_audio, strength_at_time
    from chart_ml.timing import generate_timing

    analysis = analyze_audio(audio_path)
    timing = generate_timing(
        analysis=analysis,
        level=level,
        resolution=model.resolution,
        bpm_override=bpm_override,
        timing_mode=timing_mode,
    )
    rng = random.Random(seed)
    strengths = [strength_at_time(analysis, time_seconds) for time_seconds in timing.event_times_seconds]
    patterns = _generate_patterns_for_audio_times(
        model=model,
        ticks=timing.event_ticks,
        times_seconds=timing.event_times_seconds,
        strengths=strengths,
        level=level,
        rng=rng,
    )
    chart = _patterns_to_chart(
        patterns=patterns,
        ticks=timing.event_ticks,
        timing_events=timing.timing_events,
        chart_id=chart_id,
        song_id=song_id,
        difficulty=difficulty,
        chart_author=chart_author,
        resolution=model.resolution,
    )
    chart.meta.level = level
    write_chart(chart, output_path)
    return chart
