from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path

import numpy as np

from chart_ml.audio_features import FEATURE_PROFILE_FULL, extract_frame_features, feature_layout, nearest_frame_index
from chart_ml.corpus import load_manifest_selection
from chart_ml.models import Chart, SongPackage
from chart_ml.rchart import discover_song_packages
from chart_ml.timing_utils import tick_to_seconds

CACHE_FORMAT_VERSION = 3

STATE_TO_INDEX = {
    "off": 0,
    "tap": 1,
    "hold_start": 2,
    "hold_end": 3,
}
INDEX_TO_STATE = {value: key for key, value in STATE_TO_INDEX.items()}


@dataclass(slots=True)
class SequenceExample:
    song_id: str
    chart_id: str
    audio_path: str
    feature_steps: np.ndarray
    labels: np.ndarray
    step_ticks: list[int]
    step_times_seconds: list[float]
    resolution: int
    subdivision: int
    level: float


@dataclass(slots=True)
class CacheBuildSummary:
    example_count: int
    cache_hits: int
    cache_misses: int


def _matches_filters(
    song: SongPackage,
    chart: Chart,
    song_id: str | None = None,
    chart_id: str | None = None,
    min_level: float | None = None,
    max_level: float | None = None,
    selection: set[tuple[str, str]] | None = None,
) -> bool:
    if song_id is not None and song.meta.song_id != song_id:
        return False
    if chart_id is not None and chart.meta.chart_id != chart_id:
        return False
    if min_level is not None and chart.meta.level < min_level:
        return False
    if max_level is not None and chart.meta.level > max_level:
        return False
    if selection is not None and (song.meta.song_id, chart.meta.chart_id) not in selection:
        return False
    return True


def _lane_action_at_tick(chart: Chart, tick: int, lane: int) -> str:
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
                return "hold_end"
    return "off"


def _audio_path(song: SongPackage) -> Path | None:
    candidate = Path(song.directory) / song.meta.audio_file
    return candidate if candidate.exists() else None


def build_chart_step_grid(chart: Chart, base_bpm: float, subdivision: int) -> tuple[list[int], list[float]]:
    last_tick = max((note.end_tick or note.tick) for note in chart.notes) if chart.notes else 0
    step_ticks = list(range(0, last_tick + 1, max(chart.meta.resolution // subdivision, 1)))
    if not step_ticks or step_ticks[-1] != last_tick:
        step_ticks.append(last_tick)
    step_times_seconds = [tick_to_seconds(chart, tick, fallback_bpm=base_bpm) for tick in step_ticks]
    return step_ticks, step_times_seconds


def _cache_key(
    song: SongPackage,
    chart: Chart,
    subdivision: int,
    sample_rate: int,
    hop_length: int,
    n_mels: int,
    feature_profile: str,
) -> str:
    audio_path = _audio_path(song)
    chart_path = Path(chart.path)
    payload = {
        "cache_format_version": CACHE_FORMAT_VERSION,
        "song_id": song.meta.song_id,
        "chart_id": chart.meta.chart_id,
        "audio_path": str(audio_path) if audio_path is not None else "",
        "audio_mtime_ns": audio_path.stat().st_mtime_ns if audio_path is not None and audio_path.exists() else 0,
        "chart_path": str(chart_path),
        "chart_mtime_ns": chart_path.stat().st_mtime_ns if chart_path.exists() else 0,
        "resolution": chart.meta.resolution,
        "level": chart.meta.level,
        "subdivision": subdivision,
        "sample_rate": sample_rate,
        "hop_length": hop_length,
        "n_mels": n_mels,
        "feature_profile": feature_profile,
    }
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, ensure_ascii=True).encode("utf-8")).hexdigest()
    return digest


def _cache_path(
    cache_dir: str | Path,
    song: SongPackage,
    chart: Chart,
    subdivision: int,
    sample_rate: int,
    hop_length: int,
    n_mels: int,
    feature_profile: str,
) -> Path:
    cache_root = Path(cache_dir)
    return cache_root / f"{_cache_key(song, chart, subdivision, sample_rate, hop_length, n_mels, feature_profile)}.npz"


def _save_sequence_example_cache(example: SequenceExample, cache_path: str | Path) -> None:
    path = Path(cache_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(
        path,
        song_id=np.asarray(example.song_id),
        chart_id=np.asarray(example.chart_id),
        audio_path=np.asarray(example.audio_path),
        feature_steps=example.feature_steps.astype(np.float32),
        labels=example.labels.astype(np.int64),
        step_ticks=np.asarray(example.step_ticks, dtype=np.int64),
        step_times_seconds=np.asarray(example.step_times_seconds, dtype=np.float32),
        resolution=np.asarray(example.resolution, dtype=np.int64),
        subdivision=np.asarray(example.subdivision, dtype=np.int64),
        level=np.asarray(example.level, dtype=np.float32),
    )


def _load_sequence_example_cache(cache_path: str | Path) -> SequenceExample:
    payload = np.load(Path(cache_path), allow_pickle=False)
    return SequenceExample(
        song_id=str(payload["song_id"].item()),
        chart_id=str(payload["chart_id"].item()),
        audio_path=str(payload["audio_path"].item()),
        feature_steps=np.asarray(payload["feature_steps"], dtype=np.float32),
        labels=np.asarray(payload["labels"], dtype=np.int64),
        step_ticks=np.asarray(payload["step_ticks"], dtype=np.int64).tolist(),
        step_times_seconds=np.asarray(payload["step_times_seconds"], dtype=np.float32).tolist(),
        resolution=int(payload["resolution"].item()),
        subdivision=int(payload["subdivision"].item()),
        level=float(payload["level"].item()),
    )


def build_sequence_example(
    song: SongPackage,
    chart: Chart,
    subdivision: int = 8,
    sample_rate: int = 22050,
    hop_length: int = 512,
    n_mels: int = 64,
    feature_profile: str = FEATURE_PROFILE_FULL,
) -> SequenceExample | None:
    if chart.meta.key_count != 4:
        return None

    audio_path = _audio_path(song)
    if audio_path is None:
        return None

    frame_features = extract_frame_features(
        audio_path,
        sample_rate=sample_rate,
        hop_length=hop_length,
        n_mels=n_mels,
        feature_profile=feature_profile,
    )
    mel = np.asarray(frame_features.mel_db, dtype=np.float32)
    chroma = np.asarray(frame_features.chroma, dtype=np.float32)
    onset = np.asarray(frame_features.onset_strength_values, dtype=np.float32)
    onset_bands = np.asarray(frame_features.onset_band_strengths, dtype=np.float32)
    if mel.ndim != 2 or mel.shape[0] == 0:
        return None

    mel_mean = mel.mean(axis=0, keepdims=True)
    mel_std = mel.std(axis=0, keepdims=True) + 1e-5
    mel = (mel - mel_mean) / mel_std
    if chroma.ndim == 2 and chroma.shape[1] > 0:
        chroma = (chroma - chroma.mean(axis=0, keepdims=True)) / (chroma.std(axis=0, keepdims=True) + 1e-5)
    else:
        chroma = np.zeros((mel.shape[0], 0), dtype=np.float32)
    onset = (onset - onset.mean()) / (onset.std() + 1e-5)
    if onset_bands.ndim == 2 and onset_bands.shape[1] > 0:
        onset_bands = (onset_bands - onset_bands.mean(axis=0, keepdims=True)) / (onset_bands.std(axis=0, keepdims=True) + 1e-5)
    else:
        onset_bands = np.zeros((mel.shape[0], 0), dtype=np.float32)

    feature_rows: list[np.ndarray] = []
    label_rows: list[list[int]] = []
    step_ticks, step_times_seconds = build_chart_step_grid(chart, song.meta.base_bpm, subdivision)

    for tick, time_seconds in zip(step_ticks, step_times_seconds):
        frame_index = nearest_frame_index(frame_features.frame_times_seconds, time_seconds)
        beat_position = ((tick / max(chart.meta.resolution, 1)) * subdivision) % subdivision
        feature_row = np.concatenate(
            [
                mel[frame_index],
                chroma[frame_index],
                np.array(
                    [
                        onset[frame_index],
                    ],
                    dtype=np.float32,
                ),
                onset_bands[frame_index],
                np.array(
                    [
                        float(song.meta.base_bpm) / 240.0,
                        float(beat_position) / max(subdivision - 1, 1),
                        float(chart.meta.level) / 20.0,
                    ],
                    dtype=np.float32,
                ),
            ]
        )
        labels = [STATE_TO_INDEX[_lane_action_at_tick(chart, tick, lane)] for lane in range(chart.meta.key_count)]
        feature_rows.append(feature_row)
        label_rows.append(labels)

    return SequenceExample(
        song_id=song.meta.song_id,
        chart_id=chart.meta.chart_id,
        audio_path=str(audio_path),
        feature_steps=np.stack(feature_rows).astype(np.float32),
        labels=np.asarray(label_rows, dtype=np.int64),
        step_ticks=step_ticks,
        step_times_seconds=step_times_seconds,
        resolution=chart.meta.resolution,
        subdivision=subdivision,
        level=chart.meta.level,
    )


def load_sequence_examples(
    songs_dir: str | Path,
    key_count: int = 4,
    subdivision: int = 8,
    sample_rate: int = 22050,
    hop_length: int = 512,
    n_mels: int = 64,
    feature_profile: str = FEATURE_PROFILE_FULL,
    cache_dir: str | Path | None = None,
    force_rebuild: bool = False,
    song_id: str | None = None,
    chart_id: str | None = None,
    min_level: float | None = None,
    max_level: float | None = None,
    limit: int | None = None,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> list[SequenceExample]:
    examples: list[SequenceExample] = []
    selection = load_manifest_selection(manifest_path, split=split, included_only=True) if manifest_path is not None else None
    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            if chart.meta.key_count != key_count:
                continue
            if not _matches_filters(
                song,
                chart,
                song_id=song_id,
                chart_id=chart_id,
                min_level=min_level,
                max_level=max_level,
                selection=selection,
            ):
                continue
            cache_path: Path | None = None
            if cache_dir is not None:
                cache_path = _cache_path(cache_dir, song, chart, subdivision, sample_rate, hop_length, n_mels, feature_profile)
                if cache_path.exists() and not force_rebuild:
                    examples.append(_load_sequence_example_cache(cache_path))
                    if limit is not None and len(examples) >= limit:
                        return examples
                    continue
            example = build_sequence_example(
                song,
                chart,
                subdivision=subdivision,
                sample_rate=sample_rate,
                hop_length=hop_length,
                n_mels=n_mels,
                feature_profile=feature_profile,
            )
            if example is not None:
                if cache_path is not None:
                    _save_sequence_example_cache(example, cache_path)
                examples.append(example)
                if limit is not None and len(examples) >= limit:
                    return examples
    return examples


def prepare_sequence_cache(
    songs_dir: str | Path,
    cache_dir: str | Path,
    key_count: int = 4,
    subdivision: int = 8,
    sample_rate: int = 22050,
    hop_length: int = 512,
    n_mels: int = 64,
    feature_profile: str = FEATURE_PROFILE_FULL,
    force_rebuild: bool = False,
    song_id: str | None = None,
    chart_id: str | None = None,
    min_level: float | None = None,
    max_level: float | None = None,
    limit: int | None = None,
    manifest_path: str | Path | None = None,
    split: str | None = None,
) -> CacheBuildSummary:
    example_count = 0
    cache_hits = 0
    cache_misses = 0
    selection = load_manifest_selection(manifest_path, split=split, included_only=True) if manifest_path is not None else None
    for song in discover_song_packages(songs_dir):
        for chart in song.charts:
            if chart.meta.key_count != key_count:
                continue
            if not _matches_filters(
                song,
                chart,
                song_id=song_id,
                chart_id=chart_id,
                min_level=min_level,
                max_level=max_level,
                selection=selection,
            ):
                continue
            cache_path = _cache_path(cache_dir, song, chart, subdivision, sample_rate, hop_length, n_mels, feature_profile)
            if cache_path.exists() and not force_rebuild:
                cache_hits += 1
                example_count += 1
                if limit is not None and example_count >= limit:
                    return CacheBuildSummary(example_count=example_count, cache_hits=cache_hits, cache_misses=cache_misses)
                continue
            example = build_sequence_example(
                song,
                chart,
                subdivision=subdivision,
                sample_rate=sample_rate,
                hop_length=hop_length,
                n_mels=n_mels,
                feature_profile=feature_profile,
            )
            if example is None:
                continue
            _save_sequence_example_cache(example, cache_path)
            cache_misses += 1
            example_count += 1
            if limit is not None and example_count >= limit:
                return CacheBuildSummary(example_count=example_count, cache_hits=cache_hits, cache_misses=cache_misses)
    return CacheBuildSummary(
        example_count=example_count,
        cache_hits=cache_hits,
        cache_misses=cache_misses,
    )


def build_inference_features(
    audio_path: str | Path,
    step_times_seconds: list[float],
    step_ticks: list[int],
    resolution: int,
    subdivision: int,
    level: float,
    bpm: float,
    sample_rate: int = 22050,
    hop_length: int = 512,
    n_mels: int = 64,
    feature_profile: str = FEATURE_PROFILE_FULL,
) -> np.ndarray:
    layout = feature_layout(feature_profile=feature_profile, n_mels=n_mels)
    frame_features = extract_frame_features(
        audio_path,
        sample_rate=sample_rate,
        hop_length=hop_length,
        n_mels=n_mels,
        feature_profile=feature_profile,
    )
    mel = np.asarray(frame_features.mel_db, dtype=np.float32)
    chroma = np.asarray(frame_features.chroma, dtype=np.float32)
    onset = np.asarray(frame_features.onset_strength_values, dtype=np.float32)
    onset_bands = np.asarray(frame_features.onset_band_strengths, dtype=np.float32)
    mel = (mel - mel.mean(axis=0, keepdims=True)) / (mel.std(axis=0, keepdims=True) + 1e-5)
    if layout["chroma_dim"] > 0:
        chroma = (chroma - chroma.mean(axis=0, keepdims=True)) / (chroma.std(axis=0, keepdims=True) + 1e-5)
    else:
        chroma = np.zeros((mel.shape[0], 0), dtype=np.float32)
    onset = (onset - onset.mean()) / (onset.std() + 1e-5)
    if layout["onset_band_dim"] > 0:
        onset_bands = (onset_bands - onset_bands.mean(axis=0, keepdims=True)) / (onset_bands.std(axis=0, keepdims=True) + 1e-5)
    else:
        onset_bands = np.zeros((mel.shape[0], 0), dtype=np.float32)

    feature_rows: list[np.ndarray] = []
    for tick, time_seconds in zip(step_ticks, step_times_seconds):
        frame_index = nearest_frame_index(frame_features.frame_times_seconds, time_seconds)
        beat_position = ((tick / max(resolution, 1)) * subdivision) % subdivision
        feature_rows.append(
            np.concatenate(
                [
                    mel[frame_index],
                    chroma[frame_index],
                    np.array(
                        [
                            onset[frame_index],
                        ],
                        dtype=np.float32,
                    ),
                    onset_bands[frame_index],
                    np.array(
                        [
                            float(bpm) / 240.0,
                            float(beat_position) / max(subdivision - 1, 1),
                            float(level) / 20.0,
                        ],
                        dtype=np.float32,
                    ),
                ]
            )
        )
    return np.stack(feature_rows).astype(np.float32)


def split_train_validation(examples: list[SequenceExample], validation_ratio: float = 0.15) -> tuple[list[SequenceExample], list[SequenceExample]]:
    if not examples:
        return [], []
    if validation_ratio <= 0.0:
        return list(examples), []

    ordered = sorted(examples, key=lambda item: (item.song_id, item.chart_id))
    validation_count = max(1, int(round(len(ordered) * validation_ratio)))
    if validation_count >= len(ordered):
        validation_count = max(1, len(ordered) // 5)
    train = ordered[:-validation_count] if validation_count < len(ordered) else ordered
    validation = ordered[-validation_count:] if validation_count < len(ordered) else ordered[:1]
    return train, validation


class ChunkedSequenceDataset:
    def __init__(self, examples: list[SequenceExample], chunk_length: int = 256) -> None:
        self.examples = examples
        self.chunk_length = max(chunk_length, 1)
        self.index: list[tuple[int, int, int]] = []
        for example_index, example in enumerate(examples):
            total_steps = int(example.feature_steps.shape[0])
            for start in range(0, total_steps, self.chunk_length):
                end = min(start + self.chunk_length, total_steps)
                self.index.append((example_index, start, end))

    def __len__(self) -> int:
        return len(self.index)

    def __getitem__(self, idx: int) -> dict[str, object]:
        import torch

        example_index, start, end = self.index[idx]
        example = self.examples[example_index]
        return {
            "features": torch.tensor(example.feature_steps[start:end], dtype=torch.float32),
            "labels": torch.tensor(example.labels[start:end], dtype=torch.long),
            "length": end - start,
        }


def collate_chunk_batch(batch: list[dict[str, object]]) -> dict[str, object]:
    import torch

    batch_size = len(batch)
    max_length = max(int(item["length"]) for item in batch)
    feature_dim = int(batch[0]["features"].shape[-1])

    features = torch.zeros((batch_size, max_length, feature_dim), dtype=torch.float32)
    labels = torch.full((batch_size, max_length, 4), -100, dtype=torch.long)
    lengths = torch.zeros((batch_size,), dtype=torch.long)

    for index, item in enumerate(batch):
        length = int(item["length"])
        features[index, :length] = item["features"]
        labels[index, :length] = item["labels"]
        lengths[index] = length

    return {
        "features": features,
        "labels": labels,
        "lengths": lengths,
    }
