from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(slots=True)
class SongMeta:
    song_id: str
    title: str
    artist: str
    base_bpm: float
    audio_file: str
    jacket_file: str
    preview_start_seconds: float
    song_version: int


@dataclass(slots=True)
class TimingEvent:
    event_type: str
    tick: int
    bpm: float | None = None
    numerator: int | None = None
    denominator: int | None = None


@dataclass(slots=True)
class Note:
    note_type: str
    tick: int
    lane: int
    end_tick: int | None = None


@dataclass(slots=True)
class ChartMeta:
    chart_id: str
    key_count: int
    difficulty: str
    level: float
    chart_author: str
    format_version: int
    resolution: int
    offset: int = 0
    song_id: str = ""
    is_public: bool = False


@dataclass(slots=True)
class Chart:
    path: str
    meta: ChartMeta
    timing_events: list[TimingEvent] = field(default_factory=list)
    notes: list[Note] = field(default_factory=list)


@dataclass(slots=True)
class SongPackage:
    directory: str
    meta: SongMeta
    charts: list[Chart] = field(default_factory=list)
