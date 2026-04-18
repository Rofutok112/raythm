from __future__ import annotations

from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

from chart_ml.models import Chart, SongPackage
from chart_ml.rchart import discover_song_packages


@dataclass(slots=True)
class CorpusSummary:
    manifest_path: str | None
    total_song_count: int
    total_chart_count: int
    included_chart_count: int
    excluded_chart_count: int
    split_counts: dict[str, int]
    exclusion_reasons: dict[str, int]


def _normalize_text(value: str) -> str:
    return "".join(char.lower() for char in value if char.isalnum())


def _safe_div(numerator: float, denominator: float) -> float:
    if denominator == 0.0:
        return 0.0
    return numerator / denominator


def _bpm_events(chart: Chart, fallback_bpm: float) -> list[tuple[int, float]]:
    events = [
        (event.tick, float(event.bpm))
        for event in sorted(chart.timing_events, key=lambda item: item.tick)
        if event.event_type == "bpm" and event.bpm is not None and event.bpm > 0.0
    ]
    if not events:
        return [(0, fallback_bpm)]
    if events[0][0] > 0:
        events.insert(0, (0, events[0][1]))
    return events


def _tick_to_seconds(chart: Chart, tick: int, fallback_bpm: float) -> float:
    resolution = max(chart.meta.resolution, 1)
    bpm_events = _bpm_events(chart, fallback_bpm)
    seconds = 0.0
    for index, (start_tick, bpm) in enumerate(bpm_events):
        if tick <= start_tick:
            break
        next_tick = tick
        if index + 1 < len(bpm_events):
            next_tick = min(tick, bpm_events[index + 1][0])
        if next_tick <= start_tick:
            continue
        beat_count = (next_tick - start_tick) / resolution
        seconds += beat_count * (60.0 / max(bpm, 1e-6))
        if index + 1 < len(bpm_events) and tick < bpm_events[index + 1][0]:
            break
    return seconds


def _chart_end_tick(chart: Chart) -> int:
    last_note_tick = max((note.end_tick or note.tick) for note in chart.notes) if chart.notes else 0
    last_timing_tick = max((event.tick for event in chart.timing_events), default=0)
    return max(last_note_tick, last_timing_tick)


def _same_lane_overlap_errors(chart: Chart) -> list[str]:
    errors: list[str] = []
    notes_by_lane: dict[int, list[tuple[int, int, str]]] = defaultdict(list)
    for note in chart.notes:
        end_tick = note.end_tick if note.end_tick is not None else note.tick
        notes_by_lane[note.lane].append((note.tick, end_tick, note.note_type))

    for lane, lane_notes in notes_by_lane.items():
        lane_notes.sort(key=lambda item: (item[0], item[1], item[2]))
        active_end = -1
        previous_tap_tick: int | None = None
        for start_tick, end_tick, note_type in lane_notes:
            if note_type == "tap":
                if previous_tap_tick == start_tick:
                    errors.append(f"duplicate_same_tick_tap_lane_{lane}")
                if start_tick < active_end:
                    errors.append(f"same_lane_overlap_lane_{lane}")
                previous_tap_tick = start_tick
                continue
            previous_tap_tick = None
            if end_tick <= start_tick:
                errors.append(f"non_positive_hold_lane_{lane}")
                continue
            if start_tick < active_end:
                errors.append(f"same_lane_overlap_lane_{lane}")
            active_end = max(active_end, end_tick)
    return sorted(set(errors))


def _chart_signature(chart: Chart) -> str:
    payload = {
        "resolution": chart.meta.resolution,
        "timing": [
            (
                event.event_type,
                event.tick,
                event.bpm,
                event.numerator,
                event.denominator,
            )
            for event in sorted(chart.timing_events, key=lambda item: item.tick)
        ],
        "notes": [
            (
                note.note_type,
                note.tick,
                note.lane,
                note.end_tick,
            )
            for note in sorted(chart.notes, key=lambda item: (item.tick, item.lane, item.end_tick or item.tick))
        ],
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True, ensure_ascii=True).encode("utf-8")).hexdigest()


def _song_family_signature(song: SongPackage) -> str:
    payload = {
        "title": _normalize_text(song.meta.title),
        "artist": _normalize_text(song.meta.artist),
        "audio": _normalize_text(Path(song.meta.audio_file).stem),
        "base_bpm": int(round(song.meta.base_bpm)),
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True, ensure_ascii=True).encode("utf-8")).hexdigest()[:16]


def _chart_shape_signature(chart: Chart, bin_count: int = 32) -> str:
    end_tick = max(_chart_end_tick(chart), 1)
    note_start_bins = [0] * bin_count
    hold_start_bins = [0] * bin_count
    chord_bins = [0] * bin_count
    lane_counts = [0] * max(chart.meta.key_count, 1)
    tick_counts: Counter[int] = Counter(note.tick for note in chart.notes)

    for note in chart.notes:
        ratio = min(max(note.tick / end_tick, 0.0), 1.0)
        index = min(bin_count - 1, int(ratio * bin_count))
        note_start_bins[index] += 1
        lane_counts[note.lane] += 1
        if note.note_type == "hold":
            hold_start_bins[index] += 1
        if tick_counts[note.tick] >= 2:
            chord_bins[index] += 1

    def _quantize_bins(values: list[int]) -> list[int]:
        total = max(sum(values), 1)
        return [min(15, int(round((value / total) * 15))) for value in values]

    payload = {
        "bin_count": bin_count,
        "note_bins": _quantize_bins(note_start_bins),
        "hold_bins": _quantize_bins(hold_start_bins),
        "chord_bins": _quantize_bins(chord_bins),
        "lane_distribution": _quantize_bins(lane_counts),
        "note_count_bucket": min(63, len(chart.notes) // 16),
    }
    return hashlib.sha256(json.dumps(payload, sort_keys=True, ensure_ascii=True).encode("utf-8")).hexdigest()[:16]


def _stable_hash_bucket(value: str) -> float:
    digest = hashlib.sha256(value.encode("utf-8")).hexdigest()[:16]
    return int(digest, 16) / float(16**16 - 1)


def _assign_split(song_id: str, train_ratio: float, validation_ratio: float, test_ratio: float) -> str:
    total = train_ratio + validation_ratio + test_ratio
    if total <= 0.0:
        raise ValueError("At least one split ratio must be positive")
    train_share = train_ratio / total
    validation_share = validation_ratio / total
    bucket = _stable_hash_bucket(song_id)
    if bucket < train_share:
        return "train"
    if bucket < train_share + validation_share:
        return "validation"
    return "test"


def _summarize_float_values(values: list[float]) -> dict[str, float]:
    if not values:
        return {"min": 0.0, "max": 0.0, "mean": 0.0}
    return {
        "min": float(min(values)),
        "max": float(max(values)),
        "mean": float(sum(values) / len(values)),
    }


def _top_counter_entries(counter: Counter[str], limit: int = 10) -> list[dict[str, object]]:
    return [
        {"key": key, "count": int(count)}
        for key, count in counter.most_common(limit)
    ]


def _top_labeled_counter_entries(
    counter: Counter[str],
    labels: dict[str, str],
    limit: int = 10,
) -> list[dict[str, object]]:
    return [
        {
            "key": key,
            "label": labels.get(key, key),
            "count": int(count),
        }
        for key, count in counter.most_common(limit)
    ]


def _chart_metrics(song: SongPackage, chart: Chart) -> dict[str, object]:
    note_count = len(chart.notes)
    tap_count = sum(1 for note in chart.notes if note.note_type == "tap")
    hold_count = sum(1 for note in chart.notes if note.note_type == "hold")
    end_tick = _chart_end_tick(chart)
    duration_seconds = _tick_to_seconds(chart, end_tick, song.meta.base_bpm)
    note_starts_per_tick: Counter[int] = Counter(note.tick for note in chart.notes)
    chord_starts = sum(1 for count in note_starts_per_tick.values() if count >= 2)
    chord_notes = sum(count for count in note_starts_per_tick.values() if count >= 2)
    max_chord_size = max(note_starts_per_tick.values(), default=0)
    lane_counts = Counter(note.lane for note in chart.notes)

    hold_seconds = 0.0
    for note in chart.notes:
        if note.note_type != "hold" or note.end_tick is None:
            continue
        start_seconds = _tick_to_seconds(chart, note.tick, song.meta.base_bpm)
        end_seconds = _tick_to_seconds(chart, note.end_tick, song.meta.base_bpm)
        hold_seconds += max(0.0, end_seconds - start_seconds)

    return {
        "note_count": note_count,
        "tap_count": tap_count,
        "hold_count": hold_count,
        "ln_ratio": float(_safe_div(hold_count, note_count)),
        "duration_seconds": float(duration_seconds),
        "notes_per_second": float(_safe_div(note_count, duration_seconds)),
        "chord_start_count": chord_starts,
        "max_chord_size": max_chord_size,
        "chord_note_ratio": float(_safe_div(chord_notes, note_count)),
        "hold_occupancy_ratio": float(_safe_div(hold_seconds, duration_seconds * max(chart.meta.key_count, 1))),
        "lane_note_counts": {str(lane): count for lane, count in sorted(lane_counts.items())},
    }


def _validation_report(song: SongPackage, chart: Chart) -> dict[str, list[str]]:
    errors: list[str] = []
    warnings: list[str] = []

    if chart.meta.key_count <= 0:
        errors.append("invalid_key_count")
    if chart.meta.song_id and chart.meta.song_id != song.meta.song_id:
        warnings.append("chart_song_id_mismatch")
    if not chart.notes:
        warnings.append("empty_chart")
    if not any(event.event_type == "bpm" and event.bpm is not None for event in chart.timing_events):
        warnings.append("missing_bpm_event")

    audio_path = Path(song.directory) / song.meta.audio_file
    if not audio_path.exists():
        errors.append("missing_audio_file")

    if song.meta.jacket_file:
        jacket_path = Path(song.directory) / song.meta.jacket_file
        if not jacket_path.exists():
            warnings.append("missing_jacket_file")

    for note in chart.notes:
        if note.lane < 0 or note.lane >= chart.meta.key_count:
            errors.append("lane_out_of_range")
        if note.tick < 0:
            errors.append("negative_tick")
        if note.note_type == "hold" and note.end_tick is None:
            errors.append("hold_missing_end_tick")
        if note.note_type == "hold" and note.end_tick is not None and note.end_tick <= note.tick:
            errors.append("non_positive_hold")

    errors.extend(_same_lane_overlap_errors(chart))
    return {
        "errors": sorted(set(errors)),
        "warnings": sorted(set(warnings)),
    }


def load_manifest_selection(
    manifest_path: str | Path,
    split: str | None = None,
    included_only: bool = True,
) -> set[tuple[str, str]]:
    payload = json.loads(Path(manifest_path).read_text(encoding="utf-8"))
    selection: set[tuple[str, str]] = set()
    for record in payload.get("charts", []):
        status = str(record.get("status", "excluded"))
        record_split = record.get("split")
        if included_only and status != "included":
            continue
        if split is not None and record_split != split:
            continue
        selection.add((str(record["song_id"]), str(record["chart_id"])))
    return selection


def build_corpus_manifest(
    songs_dir: str | Path,
    key_count: int | None = 4,
    train_ratio: float = 0.8,
    validation_ratio: float = 0.1,
    test_ratio: float = 0.1,
    min_note_count: int | None = None,
    max_note_count: int | None = None,
    min_ln_ratio: float | None = None,
    max_ln_ratio: float | None = None,
    min_duration_seconds: float | None = None,
    max_duration_seconds: float | None = None,
    min_notes_per_second: float | None = None,
    max_notes_per_second: float | None = None,
    exclude_duplicates: bool = True,
    max_charts_per_song_family: int | None = None,
    max_charts_per_chart_family: int | None = None,
) -> dict[str, object]:
    chart_records: list[dict[str, object]] = []
    duplicate_groups: dict[str, list[int]] = defaultdict(list)
    song_family_groups: dict[str, list[int]] = defaultdict(list)
    chart_family_groups: dict[str, list[int]] = defaultdict(list)
    song_family_labels: dict[str, str] = {}
    chart_family_labels: dict[str, str] = {}

    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            metrics = _chart_metrics(song, chart)
            validation = _validation_report(song, chart)
            signature = _chart_signature(chart)
            song_family_id = _song_family_signature(song)
            chart_shape_id = _chart_shape_signature(chart)
            chart_family_id = f"{song_family_id}:{chart_shape_id}"
            record = {
                "song_id": song.meta.song_id,
                "song_directory": song.directory,
                "title": song.meta.title,
                "artist": song.meta.artist,
                "base_bpm": song.meta.base_bpm,
                "audio_file": song.meta.audio_file,
                "chart_id": chart.meta.chart_id,
                "chart_path": chart.path,
                "difficulty": chart.meta.difficulty,
                "level": chart.meta.level,
                "key_count": chart.meta.key_count,
                "resolution": chart.meta.resolution,
                "chart_author": chart.meta.chart_author,
                "metrics": metrics,
                "validation": validation,
                "duplicate_signature": signature,
                "duplicate_group_size": 1,
                "song_family_id": song_family_id,
                "chart_family_id": chart_family_id,
                "song_family_group_size": 1,
                "chart_family_group_size": 1,
                "status": "included",
                "split": None,
                "reasons": [],
            }
            record_index = len(chart_records)
            duplicate_groups[signature].append(record_index)
            song_family_groups[song_family_id].append(record_index)
            chart_family_groups[chart_family_id].append(record_index)
            song_family_labels.setdefault(song_family_id, f"{song.meta.artist} - {song.meta.title}")
            chart_family_labels.setdefault(chart_family_id, f"{song.meta.artist} - {song.meta.title} / {chart.meta.difficulty}")
            chart_records.append(record)

    for indices in duplicate_groups.values():
        if len(indices) <= 1:
            continue
        for index in indices:
            chart_records[index]["duplicate_group_size"] = len(indices)

    for indices in song_family_groups.values():
        for index in indices:
            chart_records[index]["song_family_group_size"] = len(indices)

    for indices in chart_family_groups.values():
        for index in indices:
            chart_records[index]["chart_family_group_size"] = len(indices)

    for indices in duplicate_groups.values():
        for duplicate_index, record_index in enumerate(indices):
            record = chart_records[record_index]
            reasons = list(record["reasons"])
            validation_errors = list(record["validation"]["errors"])
            metrics = record["metrics"]

            if validation_errors:
                reasons.extend(validation_errors)
            if key_count is not None and int(record["key_count"]) != key_count:
                reasons.append("filtered_key_count")
            if min_note_count is not None and int(metrics["note_count"]) < min_note_count:
                reasons.append("below_min_note_count")
            if max_note_count is not None and int(metrics["note_count"]) > max_note_count:
                reasons.append("above_max_note_count")
            if min_ln_ratio is not None and float(metrics["ln_ratio"]) < min_ln_ratio:
                reasons.append("below_min_ln_ratio")
            if max_ln_ratio is not None and float(metrics["ln_ratio"]) > max_ln_ratio:
                reasons.append("above_max_ln_ratio")
            if min_duration_seconds is not None and float(metrics["duration_seconds"]) < min_duration_seconds:
                reasons.append("below_min_duration")
            if max_duration_seconds is not None and float(metrics["duration_seconds"]) > max_duration_seconds:
                reasons.append("above_max_duration")
            if min_notes_per_second is not None and float(metrics["notes_per_second"]) < min_notes_per_second:
                reasons.append("below_min_notes_per_second")
            if max_notes_per_second is not None and float(metrics["notes_per_second"]) > max_notes_per_second:
                reasons.append("above_max_notes_per_second")
            if exclude_duplicates and duplicate_index > 0:
                reasons.append("exact_duplicate_chart")

            record["reasons"] = sorted(set(reasons))
            if record["reasons"]:
                record["status"] = "excluded"
                record["split"] = None
            else:
                record["status"] = "included"
                record["split"] = _assign_split(
                    str(record["song_id"]),
                    train_ratio=train_ratio,
                    validation_ratio=validation_ratio,
                    test_ratio=test_ratio,
                )

    if max_charts_per_song_family is not None and max_charts_per_song_family > 0:
        for indices in song_family_groups.values():
            kept = 0
            for record_index in sorted(
                indices,
                key=lambda idx: (
                    str(chart_records[idx]["song_id"]),
                    str(chart_records[idx]["chart_id"]),
                ),
            ):
                record = chart_records[record_index]
                if record["status"] != "included":
                    continue
                kept += 1
                if kept > max_charts_per_song_family:
                    reasons = set(record["reasons"])
                    reasons.add("above_song_family_limit")
                    record["reasons"] = sorted(reasons)
                    record["status"] = "excluded"
                    record["split"] = None

    if max_charts_per_chart_family is not None and max_charts_per_chart_family > 0:
        for indices in chart_family_groups.values():
            kept = 0
            for record_index in sorted(
                indices,
                key=lambda idx: (
                    str(chart_records[idx]["song_id"]),
                    str(chart_records[idx]["chart_id"]),
                ),
            ):
                record = chart_records[record_index]
                if record["status"] != "included":
                    continue
                kept += 1
                if kept > max_charts_per_chart_family:
                    reasons = set(record["reasons"])
                    reasons.add("above_chart_family_limit")
                    record["reasons"] = sorted(reasons)
                    record["status"] = "excluded"
                    record["split"] = None

    included = [record for record in chart_records if record["status"] == "included"]
    excluded = [record for record in chart_records if record["status"] == "excluded"]
    split_counts = Counter(str(record["split"]) for record in included if record["split"] is not None)
    exclusion_reasons = Counter(reason for record in excluded for reason in record["reasons"])
    included_difficulties = Counter(str(record["difficulty"]) for record in included)
    included_song_family_counts = Counter(str(record["song_family_id"]) for record in included)
    included_chart_family_counts = Counter(str(record["chart_family_id"]) for record in included)
    ln_ratios = [float(record["metrics"]["ln_ratio"]) for record in included]
    note_counts = [float(record["metrics"]["note_count"]) for record in included]
    notes_per_second = [float(record["metrics"]["notes_per_second"]) for record in included]

    manifest = {
        "format_version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source_songs_dir": str(Path(songs_dir).resolve()),
        "filters": {
            "key_count": key_count,
            "min_note_count": min_note_count,
            "max_note_count": max_note_count,
            "min_ln_ratio": min_ln_ratio,
            "max_ln_ratio": max_ln_ratio,
            "min_duration_seconds": min_duration_seconds,
            "max_duration_seconds": max_duration_seconds,
            "min_notes_per_second": min_notes_per_second,
            "max_notes_per_second": max_notes_per_second,
            "exclude_duplicates": exclude_duplicates,
            "max_charts_per_song_family": max_charts_per_song_family,
            "max_charts_per_chart_family": max_charts_per_chart_family,
        },
        "split_config": {
            "strategy": "stable_hash_by_song_id",
            "train_ratio": train_ratio,
            "validation_ratio": validation_ratio,
            "test_ratio": test_ratio,
        },
        "summary": {
            "song_count": len({str(record["song_id"]) for record in chart_records}),
            "included_song_count": len({str(record["song_id"]) for record in included}),
            "chart_count": len(chart_records),
            "included_chart_count": len(included),
            "excluded_chart_count": len(excluded),
            "split_counts": dict(sorted(split_counts.items())),
            "difficulty_breakdown": dict(sorted(included_difficulties.items())),
            "exclusion_reasons": dict(sorted(exclusion_reasons.items())),
            "ln_ratio": _summarize_float_values(ln_ratios),
            "note_count": _summarize_float_values(note_counts),
            "notes_per_second": _summarize_float_values(notes_per_second),
            "duplicate_group_count": sum(1 for indices in duplicate_groups.values() if len(indices) > 1),
            "song_family_count": len(song_family_groups),
            "included_song_family_count": len(included_song_family_counts),
            "chart_family_count": len(chart_family_groups),
            "included_chart_family_count": len(included_chart_family_counts),
            "max_song_family_size": max((len(indices) for indices in song_family_groups.values()), default=0),
            "max_included_song_family_size": max(included_song_family_counts.values(), default=0),
            "max_chart_family_size": max((len(indices) for indices in chart_family_groups.values()), default=0),
            "max_included_chart_family_size": max(included_chart_family_counts.values(), default=0),
            "top_song_families": _top_labeled_counter_entries(included_song_family_counts, song_family_labels),
            "top_chart_families": _top_labeled_counter_entries(included_chart_family_counts, chart_family_labels),
        },
        "charts": chart_records,
    }
    return manifest


def write_corpus_manifest(manifest: dict[str, object], output_path: str | Path) -> None:
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def write_corpus_report(manifest: dict[str, object], output_path: str | Path) -> None:
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    summary = dict(manifest.get("summary", {}))
    filters = dict(manifest.get("filters", {}))
    split_config = dict(manifest.get("split_config", {}))

    lines: list[str] = []
    lines.append("# Corpus Report")
    lines.append("")
    lines.append("## Overview")
    lines.append(f"- Generated at: `{manifest.get('generated_at', '')}`")
    lines.append(f"- Source songs dir: `{manifest.get('source_songs_dir', '')}`")
    lines.append(f"- Songs: `{summary.get('song_count', 0)}`")
    lines.append(f"- Included songs: `{summary.get('included_song_count', 0)}`")
    lines.append(f"- Charts: `{summary.get('chart_count', 0)}`")
    lines.append(f"- Included charts: `{summary.get('included_chart_count', 0)}`")
    lines.append(f"- Excluded charts: `{summary.get('excluded_chart_count', 0)}`")
    lines.append("")
    lines.append("## Split")
    for split_name, count in dict(summary.get("split_counts", {})).items():
        lines.append(f"- {split_name}: `{count}`")
    lines.append("")
    lines.append("## Filters")
    for key, value in filters.items():
        lines.append(f"- {key}: `{value}`")
    lines.append("")
    lines.append("## Metric Ranges")
    for metric_name in ("ln_ratio", "note_count", "notes_per_second"):
        values = dict(summary.get(metric_name, {}))
        lines.append(
            f"- {metric_name}: min `{values.get('min', 0.0):.3f}`, mean `{values.get('mean', 0.0):.3f}`, max `{values.get('max', 0.0):.3f}`"
        )
    lines.append("")
    lines.append("## Family Concentration")
    lines.append(f"- Song families: `{summary.get('song_family_count', 0)}` total / `{summary.get('included_song_family_count', 0)}` included")
    lines.append(f"- Largest song family: `{summary.get('max_song_family_size', 0)}` total / `{summary.get('max_included_song_family_size', 0)}` included")
    lines.append(f"- Chart families: `{summary.get('chart_family_count', 0)}` total / `{summary.get('included_chart_family_count', 0)}` included")
    lines.append(f"- Largest chart family: `{summary.get('max_chart_family_size', 0)}` total / `{summary.get('max_included_chart_family_size', 0)}` included")
    lines.append("")
    top_song_families = list(summary.get("top_song_families", []))
    if top_song_families:
        lines.append("### Top Song Families")
        for item in top_song_families:
            lines.append(f"- `{item.get('label', item.get('key', ''))}` (`{item.get('key', '')}`): `{item.get('count', 0)}` charts")
        lines.append("")
    top_chart_families = list(summary.get("top_chart_families", []))
    if top_chart_families:
        lines.append("### Top Chart Families")
        for item in top_chart_families:
            lines.append(f"- `{item.get('label', item.get('key', ''))}` (`{item.get('key', '')}`): `{item.get('count', 0)}` charts")
        lines.append("")
    exclusion_reasons = dict(summary.get("exclusion_reasons", {}))
    lines.append("## Exclusion Reasons")
    if exclusion_reasons:
        for reason, count in exclusion_reasons.items():
            lines.append(f"- {reason}: `{count}`")
    else:
        lines.append("- none")
    lines.append("")
    lines.append("## Split Strategy")
    for key, value in split_config.items():
        lines.append(f"- {key}: `{value}`")
    lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def write_split_jsonl(manifest: dict[str, object], output_dir: str | Path) -> None:
    root = Path(output_dir)
    root.mkdir(parents=True, exist_ok=True)
    records_by_split: dict[str, list[dict[str, object]]] = defaultdict(list)
    for record in manifest.get("charts", []):
        if record.get("status") != "included" or record.get("split") is None:
            continue
        records_by_split[str(record["split"])].append(record)

    for split_name, records in records_by_split.items():
        path = root / f"{split_name}.jsonl"
        with path.open("w", encoding="utf-8", newline="\n") as handle:
            for record in records:
                handle.write(json.dumps(record, ensure_ascii=True))
                handle.write("\n")


def summarize_manifest(manifest: dict[str, object], manifest_path: str | None = None) -> CorpusSummary:
    summary = manifest.get("summary", {})
    return CorpusSummary(
        manifest_path=manifest_path,
        total_song_count=int(summary.get("song_count", 0)),
        total_chart_count=int(summary.get("chart_count", 0)),
        included_chart_count=int(summary.get("included_chart_count", 0)),
        excluded_chart_count=int(summary.get("excluded_chart_count", 0)),
        split_counts={str(key): int(value) for key, value in dict(summary.get("split_counts", {})).items()},
        exclusion_reasons={str(key): int(value) for key, value in dict(summary.get("exclusion_reasons", {})).items()},
    )
