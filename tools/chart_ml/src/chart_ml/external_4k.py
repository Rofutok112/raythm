from __future__ import annotations

import math
import shutil
from collections import defaultdict
from pathlib import Path
from zipfile import ZipFile

from chart_ml.models import Chart, ChartMeta, Note, SongMeta, TimingEvent
from chart_ml.rchart import write_chart, write_song_meta

_DIRECT_CHART_SUFFIX = "." + "o" + "su"
_ARCHIVE_SUFFIX = "." + "o" + "sz"


def _sanitize_identifier(value: str, fallback: str) -> str:
    result = []
    for char in value:
        if char.isalnum():
            result.append(char.lower())
        elif char in {"-", "_", "."}:
            result.append(char)
        elif char.isspace():
            result.append("_")
    cleaned = "".join(result).strip("_.")
    return cleaned or fallback


def _parse_sections(chart_path: Path) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = defaultdict(list)
    current = ""
    for raw_line in chart_path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            continue
        if current:
            sections[current].append(line)
    return sections


def _parse_sections_from_text(text: str) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = defaultdict(list)
    current = ""
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1]
            continue
        if current:
            sections[current].append(line)
    return sections


def _key_value_map(lines: list[str]) -> dict[str, str]:
    mapping: dict[str, str] = {}
    for line in lines:
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        mapping[key.strip()] = value.strip()
    return mapping


def _lane_from_x(x: int, key_count: int) -> int:
    lane_width = 512 / key_count
    return min(key_count - 1, max(0, int(x / lane_width)))


def _uninherited_timing_points(lines: list[str]) -> list[dict[str, float | int]]:
    points: list[dict[str, float | int]] = []
    for line in lines:
        parts = [part.strip() for part in line.split(",")]
        if len(parts) < 7:
            continue
        time_ms = float(parts[0])
        beat_length = float(parts[1])
        meter = int(parts[2])
        uninherited = int(parts[6])
        if uninherited != 1 or beat_length <= 0:
            continue
        bpm = 60000.0 / beat_length
        points.append({"time_ms": time_ms, "beat_length": beat_length, "meter": meter, "bpm": bpm})
    points.sort(key=lambda point: float(point["time_ms"]))
    if not points:
        points.append({"time_ms": 0.0, "beat_length": 500.0, "meter": 4, "bpm": 120.0})
    if float(points[0]["time_ms"]) > 0.0:
        first = dict(points[0])
        first["time_ms"] = 0.0
        points.insert(0, first)
    return points


def _ms_to_tick(time_ms: float, points: list[dict[str, float | int]], resolution: int) -> int:
    total_ticks = 0.0
    for index, point in enumerate(points):
        start_ms = float(point["time_ms"])
        beat_length = float(point["beat_length"])
        next_ms = time_ms
        if index + 1 < len(points):
            next_ms = min(time_ms, float(points[index + 1]["time_ms"]))
        if time_ms <= start_ms:
            break
        segment_end = max(start_ms, next_ms)
        total_ticks += ((segment_end - start_ms) / beat_length) * resolution
        if time_ms < (float(points[index + 1]["time_ms"]) if index + 1 < len(points) else math.inf):
            break
    return int(round(total_ticks))


def _parse_four_key_chart_sections(sections: dict[str, list[str]], source_name: str, resolution: int = 480) -> tuple[SongMeta, Chart]:
    general = _key_value_map(sections.get("General", []))
    metadata = _key_value_map(sections.get("Metadata", []))
    difficulty = _key_value_map(sections.get("Difficulty", []))

    if int(general.get("Mode", "0")) != 3:
        raise ValueError(f"Not a supported 4-key chart: {source_name}")

    key_count = int(float(difficulty.get("CircleSize", "0")))
    if key_count != 4:
        raise ValueError(f"Expected 4-key chart, found {key_count}K: {source_name}")

    audio_file = general.get("AudioFilename", "")
    title = metadata.get("TitleUnicode", metadata.get("Title", Path(source_name).stem))
    artist = metadata.get("ArtistUnicode", metadata.get("Artist", "Unknown"))
    creator = metadata.get("Creator", "Unknown")
    version = metadata.get("Version", "external")
    set_id = metadata.get("BeatmapSetID", "0")
    chart_source_id = metadata.get("BeatmapID", "0")

    timing_points = _uninherited_timing_points(sections.get("TimingPoints", []))
    base_bpm = float(timing_points[0]["bpm"])
    preview_ms = float(general.get("PreviewTime", "-1"))
    preview_seconds = max(0.0, preview_ms / 1000.0) if preview_ms >= 0.0 else 0.0

    song_id = _sanitize_identifier(f"ext4k_{set_id}_{title}_{artist}", f"ext4k_{set_id}")
    chart_id = _sanitize_identifier(f"ext4k_{chart_source_id}_{version}", f"ext4k_chart_{chart_source_id}")

    notes: list[Note] = []
    for line in sections.get("HitObjects", []):
        parts = line.split(",")
        if len(parts) < 5:
            continue
        x = int(parts[0])
        time_ms = float(parts[2])
        note_type_bits = int(parts[3])
        lane = _lane_from_x(x, key_count)
        tick = _ms_to_tick(time_ms, timing_points, resolution)
        if note_type_bits & 128:
            extra = parts[5] if len(parts) > 5 else ""
            end_time_token = extra.split(":", 1)[0]
            end_ms = float(end_time_token)
            end_tick = max(tick + 1, _ms_to_tick(end_ms, timing_points, resolution))
            notes.append(Note(note_type="hold", tick=tick, lane=lane, end_tick=end_tick))
        else:
            notes.append(Note(note_type="tap", tick=tick, lane=lane))

    chart = Chart(
        path=source_name,
        meta=ChartMeta(
            chart_id=chart_id,
            key_count=4,
            difficulty=version,
            level=float(difficulty.get("OverallDifficulty", difficulty.get("HPDrainRate", "5"))),
            chart_author=creator,
            format_version=1,
            resolution=resolution,
            offset=0,
            song_id=song_id,
            is_public=False,
        ),
        timing_events=[
            TimingEvent(event_type="meter", tick=0, numerator=int(timing_points[0]["meter"]), denominator=4),
            *[
                TimingEvent(event_type="bpm", tick=_ms_to_tick(float(point["time_ms"]), timing_points, resolution), bpm=float(point["bpm"]))
                for point in timing_points
            ],
        ],
        notes=sorted(notes, key=lambda note: (note.tick, note.lane, note.end_tick or note.tick)),
    )

    song = SongMeta(
        song_id=song_id,
        title=title,
        artist=artist,
        base_bpm=base_bpm,
        audio_file=audio_file,
        jacket_file="jacket.png",
        preview_start_seconds=preview_seconds,
        song_version=1,
    )
    return song, chart


def parse_four_key_chart(chart_path: str | Path, resolution: int = 480) -> tuple[SongMeta, Chart]:
    path = Path(chart_path)
    sections = _parse_sections(path)
    song, chart = _parse_four_key_chart_sections(sections, str(path), resolution=resolution)
    chart.path = str(path)
    return song, chart


def discover_direct_chart_files(source_root: str | Path) -> list[Path]:
    root = Path(source_root)
    charts: list[Path] = []
    for path in root.rglob(f"*{_DIRECT_CHART_SUFFIX}"):
        try:
            sections = _parse_sections(path)
            general = _key_value_map(sections.get("General", []))
            difficulty = _key_value_map(sections.get("Difficulty", []))
            if int(general.get("Mode", "0")) == 3 and int(float(difficulty.get("CircleSize", "0"))) == 4:
                charts.append(path)
        except Exception:
            continue
    return sorted(charts)


def _copy_first_background(set_dir: Path, song_dir: Path) -> str:
    background_candidates = sorted([path for path in set_dir.iterdir() if path.suffix.lower() in {".png", ".jpg", ".jpeg"}])
    if not background_candidates:
        return "jacket.png"
    source = background_candidates[0]
    target_name = "jacket.png" if source.suffix.lower() == ".png" else "jacket" + source.suffix.lower()
    shutil.copy2(source, song_dir / target_name)
    return target_name


def convert_direct_chart_sets_to_song_packages(source_root: str | Path, output_root: str | Path, resolution: int = 480) -> int:
    output_base = Path(output_root)
    output_base.mkdir(parents=True, exist_ok=True)

    charts_by_song: dict[str, list[tuple[SongMeta, Chart, Path]]] = defaultdict(list)
    for chart_path in discover_direct_chart_files(source_root):
        try:
            song_meta, chart = parse_four_key_chart(chart_path, resolution=resolution)
        except Exception:
            continue
        charts_by_song[song_meta.song_id].append((song_meta, chart, chart_path.parent))

    converted = 0
    for song_id, entries in charts_by_song.items():
        primary_song = entries[0][0]
        set_dir = entries[0][2]
        song_dir = output_base / song_id
        charts_dir = song_dir / "charts"
        charts_dir.mkdir(parents=True, exist_ok=True)

        audio_source = set_dir / primary_song.audio_file
        audio_target_name = "audio" + audio_source.suffix.lower() if audio_source.suffix else "audio.mp3"
        if audio_source.exists():
            shutil.copy2(audio_source, song_dir / audio_target_name)
        else:
            audio_target_name = primary_song.audio_file

        jacket_target_name = _copy_first_background(set_dir, song_dir)

        write_song_meta(primary_song, song_dir / "song.json", audio_target_name, jacket_target_name)

        for _, chart, _ in entries:
            write_chart(chart, charts_dir / f"{chart.meta.chart_id}.rchart")
            converted += 1

    return converted


def discover_chart_bundle_files(source_root: str | Path) -> list[Path]:
    return sorted(Path(source_root).rglob(f"*{_ARCHIVE_SUFFIX}"))


def _decode_text(data: bytes) -> str:
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("utf-8", errors="replace")


def convert_chart_bundle_sets_to_song_packages(source_root: str | Path, output_root: str | Path, resolution: int = 480) -> int:
    output_base = Path(output_root)
    output_base.mkdir(parents=True, exist_ok=True)

    converted = 0
    for bundle_path in discover_chart_bundle_files(source_root):
        with ZipFile(bundle_path) as archive:
            entries: list[tuple[SongMeta, Chart, str]] = []
            for member in archive.namelist():
                if not member.lower().endswith(_DIRECT_CHART_SUFFIX):
                    continue
                sections = _parse_sections_from_text(_decode_text(archive.read(member)))
                try:
                    song_meta, chart = _parse_four_key_chart_sections(
                        sections,
                        f"{bundle_path.name}:{member}",
                        resolution=resolution,
                    )
                except Exception:
                    continue
                chart.path = f"{bundle_path}!{member}"
                entries.append((song_meta, chart, member))

            four_key_entries: list[tuple[SongMeta, Chart, str]] = []
            for song_meta, chart, member in entries:
                if chart.meta.key_count == 4:
                    four_key_entries.append((song_meta, chart, member))
            if not four_key_entries:
                continue

            primary_song = four_key_entries[0][0]
            song_dir = output_base / primary_song.song_id
            charts_dir = song_dir / "charts"
            charts_dir.mkdir(parents=True, exist_ok=True)

            audio_target_name = "audio.mp3"
            audio_member_name = primary_song.audio_file
            if audio_member_name in archive.namelist():
                source_suffix = Path(audio_member_name).suffix.lower() or ".mp3"
                audio_target_name = "audio" + source_suffix
                (song_dir / audio_target_name).write_bytes(archive.read(audio_member_name))

            image_members = [name for name in archive.namelist() if Path(name).suffix.lower() in {".png", ".jpg", ".jpeg"}]
            jacket_target_name = "jacket.png"
            if image_members:
                source_name = sorted(image_members)[0]
                suffix = Path(source_name).suffix.lower()
                jacket_target_name = "jacket.png" if suffix == ".png" else "jacket" + suffix
                (song_dir / jacket_target_name).write_bytes(archive.read(source_name))

            write_song_meta(primary_song, song_dir / "song.json", audio_target_name, jacket_target_name)

            for _, chart, _ in four_key_entries:
                write_chart(chart, charts_dir / f"{chart.meta.chart_id}.rchart")
                converted += 1

    return converted
