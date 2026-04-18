from __future__ import annotations

from dataclasses import dataclass

from chart_ml.models import Chart, TimingEvent


@dataclass(slots=True)
class BpmSegment:
    start_tick: int
    bpm: float
    start_seconds: float


def bpm_segments(chart: Chart, fallback_bpm: float | None = None) -> list[BpmSegment]:
    bpm_events = [event for event in sorted(chart.timing_events, key=lambda item: item.tick) if event.event_type == "bpm" and event.bpm]
    if not bpm_events:
        bpm_value = float(fallback_bpm or 120.0)
        bpm_events = [TimingEvent(event_type="bpm", tick=0, bpm=bpm_value)]
    elif bpm_events[0].tick != 0:
        bpm_events.insert(0, TimingEvent(event_type="bpm", tick=0, bpm=float(bpm_events[0].bpm)))

    segments: list[BpmSegment] = []
    elapsed_seconds = 0.0
    previous_tick = bpm_events[0].tick
    previous_bpm = float(bpm_events[0].bpm or fallback_bpm or 120.0)
    segments.append(BpmSegment(start_tick=previous_tick, bpm=previous_bpm, start_seconds=0.0))

    for event in bpm_events[1:]:
        tick_delta = event.tick - previous_tick
        elapsed_seconds += ticks_to_seconds(tick_delta, chart.meta.resolution, previous_bpm)
        previous_tick = event.tick
        previous_bpm = float(event.bpm or previous_bpm)
        segments.append(BpmSegment(start_tick=previous_tick, bpm=previous_bpm, start_seconds=elapsed_seconds))

    return segments


def ticks_to_seconds(tick_delta: int, resolution: int, bpm: float) -> float:
    beats = float(tick_delta) / max(resolution, 1)
    seconds_per_beat = 60.0 / max(bpm, 1e-6)
    return beats * seconds_per_beat


def tick_to_seconds(chart: Chart, tick: int, fallback_bpm: float | None = None) -> float:
    segments = bpm_segments(chart, fallback_bpm=fallback_bpm)
    active = segments[0]
    for segment in segments[1:]:
        if tick < segment.start_tick:
            break
        active = segment
    return active.start_seconds + ticks_to_seconds(tick - active.start_tick, chart.meta.resolution, active.bpm)

