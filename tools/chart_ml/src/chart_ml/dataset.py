from __future__ import annotations

import json
from collections import Counter
from dataclasses import asdict
from pathlib import Path

from chart_ml.corpus import load_manifest_selection
from chart_ml.models import Chart, Note, SongPackage
from chart_ml.rchart import discover_song_packages

LaneState = str


def _note_record(note: Note) -> dict[str, int | str | None]:
    return {
        "note_type": note.note_type,
        "tick": note.tick,
        "lane": note.lane,
        "end_tick": note.end_tick,
    }


def chart_record(song: SongPackage, chart: Chart) -> dict[str, object]:
    tap_count = sum(1 for note in chart.notes if note.note_type == "tap")
    hold_count = sum(1 for note in chart.notes if note.note_type == "hold")
    return {
        "song_directory": song.directory,
        "song": asdict(song.meta),
        "chart": {
            **asdict(chart.meta),
            "path": chart.path,
            "note_count": len(chart.notes),
            "tap_count": tap_count,
            "hold_count": hold_count,
        },
        "timing_events": [asdict(event) for event in chart.timing_events],
        "notes": [_note_record(note) for note in chart.notes],
    }


def _event_ticks(chart: Chart) -> list[int]:
    ticks = {event.tick for event in chart.timing_events}
    for note in chart.notes:
        ticks.add(note.tick)
        if note.note_type == "hold" and note.end_tick is not None:
            ticks.add(note.end_tick)
    return sorted(ticks)


def _lane_state_at_tick(chart: Chart, tick: int, lane: int) -> LaneState:
    state: LaneState = "off"
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


def _window_state(chart: Chart, tick: int) -> list[LaneState]:
    return [_lane_state_at_tick(chart, tick, lane) for lane in range(chart.meta.key_count)]


def pattern_windows(song: SongPackage, chart: Chart, span: int = 4) -> list[dict[str, object]]:
    if chart.meta.key_count != 4:
        return []

    ticks = _event_ticks(chart)
    examples: list[dict[str, object]] = []
    for index in range(len(ticks)):
        start = max(0, index - span + 1)
        context_ticks = ticks[start : index + 1]
        while len(context_ticks) < span:
            context_ticks.insert(0, context_ticks[0] if context_ticks else 0)

        examples.append(
            {
                "song_id": song.meta.song_id,
                "chart_id": chart.meta.chart_id,
                "difficulty": chart.meta.difficulty,
                "level": chart.meta.level,
                "chart_path": chart.path,
                "event_index": index,
                "current_tick": ticks[index],
                "context_ticks": context_ticks,
                "lane_states": [_window_state(chart, tick) for tick in context_ticks],
            }
        )

    return examples


def write_jsonl(records: list[dict[str, object]], output_path: str | Path) -> None:
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        for record in records:
            handle.write(json.dumps(record, ensure_ascii=True))
            handle.write("\n")


def _load_selection(manifest_path: str | Path | None, split: str | None) -> set[tuple[str, str]] | None:
    if manifest_path is None:
        return None
    return load_manifest_selection(manifest_path, split=split, included_only=True)


def _is_selected(song_id: str, chart_id: str, selection: set[tuple[str, str]] | None) -> bool:
    if selection is None:
        return True
    return (song_id, chart_id) in selection


def extract_chart_dataset(
    songs_dir: str | Path,
    key_count: int | None = None,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    selection = _load_selection(manifest_path, split)
    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            if key_count is not None and chart.meta.key_count != key_count:
                continue
            if not _is_selected(song.meta.song_id, chart.meta.chart_id, selection):
                continue
            records.append(chart_record(song, chart))
    return records


def extract_window_dataset(
    songs_dir: str | Path,
    key_count: int = 4,
    span: int = 4,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    selection = _load_selection(manifest_path, split)
    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            if chart.meta.key_count != key_count:
                continue
            if not _is_selected(song.meta.song_id, chart.meta.chart_id, selection):
                continue
            records.extend(pattern_windows(song, chart, span=span))
    return records


def dataset_stats(
    songs_dir: str | Path,
    key_count: int | None = None,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> dict[str, object]:
    packages = discover_song_packages(songs_dir)
    selection = _load_selection(manifest_path, split)
    chart_count = 0
    note_count = 0
    difficulty_counter: Counter[str] = Counter()
    key_counter: Counter[int] = Counter()
    hold_count = 0
    tap_count = 0
    selected_song_ids: set[str] = set()

    for song in packages:
        for chart in song.charts:
            if key_count is not None and chart.meta.key_count != key_count:
                continue
            if not _is_selected(song.meta.song_id, chart.meta.chart_id, selection):
                continue
            chart_count += 1
            selected_song_ids.add(song.meta.song_id)
            key_counter[chart.meta.key_count] += 1
            difficulty_counter[chart.meta.difficulty] += 1
            for note in chart.notes:
                note_count += 1
                if note.note_type == "hold":
                    hold_count += 1
                else:
                    tap_count += 1

    return {
        "song_count": len(selected_song_ids) if selection is not None else len(packages),
        "chart_count": chart_count,
        "note_count": note_count,
        "tap_count": tap_count,
        "hold_count": hold_count,
        "difficulty_breakdown": dict(sorted(difficulty_counter.items())),
        "key_count_breakdown": dict(sorted(key_counter.items())),
    }
