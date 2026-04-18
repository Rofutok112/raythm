from __future__ import annotations

import json
import shutil
from pathlib import Path

from chart_ml.models import Chart, ChartMeta, Note, SongMeta, SongPackage, TimingEvent


def _strip_comment(line: str) -> str:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return ""
    return stripped


def _parse_bool(value: str) -> bool:
    return value.strip().lower() == "true"


def _parse_song_meta(song_json_path: Path) -> SongMeta:
    data = json.loads(song_json_path.read_text(encoding="utf-8"))
    preview_seconds = data.get("chorusStartSeconds", data.get("previewStartSeconds"))
    if preview_seconds is None and "previewStartMs" in data:
        preview_seconds = float(data["previewStartMs"]) / 1000.0

    return SongMeta(
        song_id=str(data["songId"]),
        title=str(data["title"]),
        artist=str(data["artist"]),
        base_bpm=float(data["baseBpm"]),
        audio_file=str(data["audioFile"]),
        jacket_file=str(data["jacketFile"]),
        preview_start_seconds=float(preview_seconds or 0.0),
        song_version=int(data["songVersion"]),
    )


def parse_chart(chart_path: str | Path) -> Chart:
    path = Path(chart_path)
    sections: dict[str, list[str]] = {}
    current_section = ""

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = _strip_comment(raw_line)
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            current_section = line[1:-1]
            sections.setdefault(current_section, [])
            continue
        if not current_section:
            raise ValueError(f"Entry exists before section header in {path}")
        sections[current_section].append(line)

    required = {"Metadata", "Timing", "Notes"}
    missing = sorted(required.difference(sections))
    if missing:
        raise ValueError(f"Missing sections in {path}: {', '.join(missing)}")

    metadata_map: dict[str, str] = {}
    for entry in sections["Metadata"]:
        if "=" in entry:
            key, value = entry.split("=", 1)
        else:
            key, value = entry.split(",", 1)
        metadata_map[key.strip()] = value.strip()

    meta = ChartMeta(
        chart_id=metadata_map["chartId"],
        key_count=int(metadata_map["keyCount"]),
        difficulty=metadata_map["difficulty"],
        level=float(metadata_map["level"]),
        chart_author=metadata_map["chartAuthor"],
        format_version=int(metadata_map["formatVersion"]),
        resolution=int(metadata_map["resolution"]),
        offset=int(metadata_map.get("offset", "0")),
        song_id=metadata_map.get("songId", ""),
        is_public=_parse_bool(metadata_map.get("isPublic", "false")),
    )

    timing_events: list[TimingEvent] = []
    for entry in sections["Timing"]:
        event_type, tick_token, payload = [token.strip() for token in entry.split(",", 2)]
        tick = int(tick_token)
        if event_type == "bpm":
            timing_events.append(TimingEvent(event_type=event_type, tick=tick, bpm=float(payload)))
        elif event_type == "meter":
            numerator_token, denominator_token = payload.split("/", 1)
            timing_events.append(
                TimingEvent(
                    event_type=event_type,
                    tick=tick,
                    numerator=int(numerator_token.strip()),
                    denominator=int(denominator_token.strip()),
                )
            )
        else:
            raise ValueError(f"Unknown timing event type {event_type!r} in {path}")

    notes: list[Note] = []
    for entry in sections["Notes"]:
        tokens = [token.strip() for token in entry.split(",")]
        note_type = tokens[0]
        if note_type == "tap":
            _, tick_token, lane_token = tokens
            notes.append(Note(note_type=note_type, tick=int(tick_token), lane=int(lane_token)))
        elif note_type == "hold":
            _, tick_token, lane_token, end_tick_token = tokens
            notes.append(
                Note(
                    note_type=note_type,
                    tick=int(tick_token),
                    lane=int(lane_token),
                    end_tick=int(end_tick_token),
                )
            )
        else:
            raise ValueError(f"Unknown note type {note_type!r} in {path}")

    notes.sort(key=lambda note: (note.tick, note.lane, note.end_tick or note.tick))
    timing_events.sort(key=lambda event: event.tick)
    return Chart(path=str(path), meta=meta, timing_events=timing_events, notes=notes)


def load_song_package(song_dir: str | Path) -> SongPackage:
    root = Path(song_dir)
    song_json_path = root / "song.json"
    charts_dir = root / "charts"

    if not song_json_path.exists():
        raise FileNotFoundError(f"Missing song.json: {song_json_path}")

    meta = _parse_song_meta(song_json_path)
    charts = []
    if charts_dir.exists():
        for chart_path in sorted(charts_dir.glob("*.rchart")):
            charts.append(parse_chart(chart_path))

    return SongPackage(directory=str(root), meta=meta, charts=charts)


def discover_song_packages(songs_dir: str | Path) -> list[SongPackage]:
    root = Path(songs_dir)
    packages: list[SongPackage] = []
    for child in sorted(root.iterdir()):
        if child.is_dir() and (child / "song.json").exists():
            packages.append(load_song_package(child))
    return packages


def serialize_chart(chart: Chart) -> str:
    lines: list[str] = []
    lines.append("[Metadata]")
    lines.append(f"chartId={chart.meta.chart_id}")
    lines.append(f"keyCount={chart.meta.key_count}")
    lines.append(f"difficulty={chart.meta.difficulty}")
    lines.append(f"level={chart.meta.level:.1f}")
    lines.append(f"chartAuthor={chart.meta.chart_author}")
    lines.append(f"formatVersion={chart.meta.format_version}")
    lines.append(f"resolution={chart.meta.resolution}")
    lines.append(f"offset={chart.meta.offset}")
    if chart.meta.song_id:
        lines.append(f"songId={chart.meta.song_id}")
    lines.append(f"isPublic={'true' if chart.meta.is_public else 'false'}")
    lines.append("")

    lines.append("[Timing]")
    for event in sorted(chart.timing_events, key=lambda item: item.tick):
        if event.event_type == "bpm":
            lines.append(f"bpm,{event.tick},{event.bpm:g}")
        elif event.event_type == "meter":
            lines.append(f"meter,{event.tick},{event.numerator}/{event.denominator}")
        else:
            raise ValueError(f"Unsupported timing event type: {event.event_type}")
    lines.append("")

    lines.append("[Notes]")
    for note in sorted(chart.notes, key=lambda item: (item.tick, item.lane, item.end_tick or item.tick)):
        if note.note_type == "tap":
            lines.append(f"tap,{note.tick},{note.lane}")
        elif note.note_type == "hold":
            if note.end_tick is None:
                raise ValueError("Hold note is missing end_tick")
            lines.append(f"hold,{note.tick},{note.lane},{note.end_tick}")
        else:
            raise ValueError(f"Unsupported note type: {note.note_type}")
    lines.append("")
    return "\n".join(lines)


def write_chart(chart: Chart, chart_path: str | Path) -> None:
    path = Path(chart_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(serialize_chart(chart), encoding="utf-8", newline="\n")


def write_song_meta(song_meta: SongMeta, song_json_path: str | Path, audio_file: str, jacket_file: str) -> None:
    path = Path(song_json_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "songId": song_meta.song_id,
        "title": song_meta.title,
        "artist": song_meta.artist,
        "baseBpm": song_meta.base_bpm,
        "audioFile": audio_file,
        "jacketFile": jacket_file,
        "chorusStartSeconds": song_meta.preview_start_seconds,
        "songVersion": song_meta.song_version,
    }
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def write_song_package(
    song_meta: SongMeta,
    chart: Chart,
    song_dir: str | Path,
    source_audio_path: str | Path,
    source_jacket_path: str | Path | None = None,
    chart_file_name: str | None = None,
) -> tuple[Path, Path]:
    root = Path(song_dir)
    charts_dir = root / "charts"
    charts_dir.mkdir(parents=True, exist_ok=True)

    audio_source = Path(source_audio_path)
    audio_target_name = audio_source.name
    shutil.copy2(audio_source, root / audio_target_name)

    jacket_target_name = ""
    if source_jacket_path is not None:
        jacket_source = Path(source_jacket_path)
        if jacket_source.exists():
            jacket_target_name = jacket_source.name
            shutil.copy2(jacket_source, root / jacket_target_name)

    write_song_meta(song_meta, root / "song.json", audio_target_name, jacket_target_name)

    chart_name = chart_file_name or f"{chart.meta.chart_id}.rchart"
    chart_path = charts_dir / chart_name
    write_chart(chart, chart_path)
    return root / "song.json", chart_path
