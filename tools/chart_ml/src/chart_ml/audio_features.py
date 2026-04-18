from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

FEATURE_PROFILE_LEGACY = "legacy"
FEATURE_PROFILE_FULL = "full"


@dataclass(slots=True)
class AudioAnalysis:
    path: str
    sample_rate: int
    channel_count: int
    duration_seconds: float
    active_start_seconds: float
    active_end_seconds: float
    tempo_bpm: float
    beat_times: list[float]
    onset_times: list[float]
    onset_strength_times: list[float]
    onset_strength_values: list[float]


@dataclass(slots=True)
class FrameFeatures:
    path: str
    sample_rate: int
    hop_length: int
    feature_profile: str
    channel_count: int
    mel_stream_names: list[str]
    chroma_dim: int
    onset_band_names: list[str]
    frame_times_seconds: list[float]
    mel_db: list[list[float]]
    chroma: list[list[float]]
    onset_strength_values: list[float]
    onset_band_strengths: list[list[float]]


def _ensure_stereo_waveform(waveform):
    import numpy as np

    if waveform.ndim == 1:
        return np.stack([waveform, waveform], axis=0), 1
    if waveform.ndim == 2 and waveform.shape[0] >= 2:
        return waveform[:2], waveform.shape[0]
    if waveform.ndim == 2 and waveform.shape[0] == 1:
        return np.concatenate([waveform, waveform], axis=0), 1
    raise ValueError("Unsupported audio waveform shape")


def _pad_or_trim_1d(values, length: int):
    import numpy as np

    array = np.asarray(values, dtype=np.float32).reshape(-1)
    if array.shape[0] < length:
        return np.pad(array, (0, length - array.shape[0]))
    if array.shape[0] > length:
        return array[:length]
    return array


def _pad_or_trim_2d(values, length: int):
    import numpy as np

    array = np.asarray(values, dtype=np.float32)
    if array.ndim == 1:
        array = array.reshape(-1, 1)
    if array.shape[0] < length:
        pad = np.zeros((length - array.shape[0], array.shape[1]), dtype=np.float32)
        return np.vstack([array, pad])
    if array.shape[0] > length:
        return array[:length]
    return array


def _load_mid_waveform(audio_path: str | Path, sample_rate: int = 22050):
    import librosa

    stereo_waveform, sr = librosa.load(Path(audio_path), sr=sample_rate, mono=False)
    stereo_waveform, original_channel_count = _ensure_stereo_waveform(stereo_waveform)
    mid = 0.5 * (stereo_waveform[0] + stereo_waveform[1])
    return stereo_waveform, mid, sr, original_channel_count


def _band_onset_strengths(waveform, sr: int, hop_length: int, n_fft: int):
    import librosa
    import numpy as np

    stft_magnitude = np.abs(librosa.stft(y=waveform, n_fft=n_fft, hop_length=hop_length))
    frequencies = librosa.fft_frequencies(sr=sr, n_fft=n_fft)
    band_definitions = [
        ("low", frequencies < 200.0),
        ("mid", (frequencies >= 200.0) & (frequencies < 2000.0)),
        ("high", frequencies >= 2000.0),
    ]

    band_names: list[str] = []
    band_values: list[np.ndarray] = []
    for name, mask in band_definitions:
        band_names.append(name)
        if not mask.any():
            band_values.append(np.zeros((stft_magnitude.shape[1],), dtype=np.float32))
            continue
        band_spec = stft_magnitude[mask]
        onset = librosa.onset.onset_strength(S=band_spec, sr=sr, hop_length=hop_length).astype(np.float32)
        band_values.append(onset)
    return band_names, band_values


def feature_layout(
    feature_profile: str,
    n_mels: int,
    chroma_bins: int = 12,
    band_onset_count: int = 3,
) -> dict[str, int]:
    if feature_profile == FEATURE_PROFILE_LEGACY:
        mel_stream_count = 1
        chroma_dim = 0
        onset_band_dim = 0
    elif feature_profile == FEATURE_PROFILE_FULL:
        mel_stream_count = 4
        chroma_dim = chroma_bins
        onset_band_dim = band_onset_count
    else:
        raise ValueError(f"Unknown feature profile: {feature_profile}")

    mel_feature_dim = n_mels * mel_stream_count
    total_dim = mel_feature_dim + chroma_dim + 1 + onset_band_dim + 3
    return {
        "mel_stream_count": mel_stream_count,
        "mel_feature_dim": mel_feature_dim,
        "chroma_dim": chroma_dim,
        "onset_band_dim": onset_band_dim,
        "scalar_feature_dim": 3,
        "overall_onset_index": mel_feature_dim + chroma_dim,
        "band_onset_start_index": mel_feature_dim + chroma_dim + 1,
        "total_dim": total_dim,
    }


def analyze_audio(audio_path: str | Path, sample_rate: int = 22050) -> AudioAnalysis:
    import librosa
    import numpy as np

    path = Path(audio_path)
    stereo_waveform, waveform, sr, original_channel_count = _load_mid_waveform(path, sample_rate=sample_rate)
    if waveform.size == 0 or stereo_waveform.size == 0:
        raise ValueError(f"Audio file is empty: {path}")

    onset_env = librosa.onset.onset_strength(y=waveform, sr=sr)
    tempo, beat_frames = librosa.beat.beat_track(y=waveform, sr=sr, onset_envelope=onset_env, trim=False)
    onset_frames = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr, units="frames", backtrack=False)
    non_silent_intervals = librosa.effects.split(waveform, top_db=35)

    tempo_value = float(np.asarray(tempo).reshape(-1)[0]) if np.asarray(tempo).size else 0.0

    beat_times = librosa.frames_to_time(beat_frames, sr=sr).tolist()
    onset_times = librosa.frames_to_time(onset_frames, sr=sr).tolist()
    onset_strength_times = librosa.frames_to_time(np.arange(len(onset_env)), sr=sr).tolist()
    onset_strength_values = onset_env.astype(float).tolist()
    duration_seconds = float(librosa.get_duration(y=waveform, sr=sr))
    if len(non_silent_intervals) > 0:
        active_start_seconds = float(non_silent_intervals[0][0] / sr)
        active_end_seconds = float(non_silent_intervals[-1][1] / sr)
    else:
        active_start_seconds = 0.0
        active_end_seconds = duration_seconds

    return AudioAnalysis(
        path=str(path),
        sample_rate=sr,
        channel_count=original_channel_count,
        duration_seconds=duration_seconds,
        active_start_seconds=active_start_seconds,
        active_end_seconds=active_end_seconds,
        tempo_bpm=tempo_value,
        beat_times=beat_times,
        onset_times=onset_times,
        onset_strength_times=onset_strength_times,
        onset_strength_values=onset_strength_values,
    )


def extract_frame_features(
    audio_path: str | Path,
    sample_rate: int = 22050,
    hop_length: int = 512,
    n_fft: int = 2048,
    n_mels: int = 64,
    feature_profile: str = FEATURE_PROFILE_FULL,
) -> FrameFeatures:
    import librosa
    import numpy as np

    path = Path(audio_path)
    stereo_waveform, mid_waveform, sr, original_channel_count = _load_mid_waveform(path, sample_rate=sample_rate)
    if mid_waveform.size == 0 or stereo_waveform.size == 0:
        raise ValueError(f"Audio file is empty: {path}")

    if feature_profile == FEATURE_PROFILE_LEGACY:
        mel_stream_names = ["mono"]
        stream_waveforms = [mid_waveform]
    elif feature_profile == FEATURE_PROFILE_FULL:
        left = stereo_waveform[0]
        right = stereo_waveform[1]
        side = 0.5 * (left - right)
        mel_stream_names = ["left", "right", "mid", "side"]
        stream_waveforms = [left, right, mid_waveform, side]
    else:
        raise ValueError(f"Unknown feature profile: {feature_profile}")

    mel_streams: list[np.ndarray] = []
    for stream_waveform in stream_waveforms:
        mel = librosa.feature.melspectrogram(
            y=stream_waveform,
            sr=sr,
            n_fft=n_fft,
            hop_length=hop_length,
            n_mels=n_mels,
            power=2.0,
        )
        mel_streams.append(librosa.power_to_db(mel, ref=np.max).T.astype(np.float32))

    mel_db = np.concatenate(mel_streams, axis=1)
    frame_count = mel_db.shape[0]
    onset_env = librosa.onset.onset_strength(y=mid_waveform, sr=sr, hop_length=hop_length).astype(np.float32)
    onset_env = _pad_or_trim_1d(onset_env, frame_count)

    if feature_profile == FEATURE_PROFILE_FULL:
        chroma = librosa.feature.chroma_stft(y=mid_waveform, sr=sr, hop_length=hop_length, n_fft=n_fft).T.astype(np.float32)
        chroma = _pad_or_trim_2d(chroma, frame_count)
        onset_band_names, onset_band_values = _band_onset_strengths(mid_waveform, sr=sr, hop_length=hop_length, n_fft=n_fft)
        onset_bands = np.stack([_pad_or_trim_1d(values, frame_count) for values in onset_band_values], axis=1).astype(np.float32)
    else:
        chroma = np.zeros((frame_count, 0), dtype=np.float32)
        onset_band_names = []
        onset_bands = np.zeros((frame_count, 0), dtype=np.float32)

    frame_times = librosa.frames_to_time(np.arange(frame_count), sr=sr, hop_length=hop_length).astype(float)
    return FrameFeatures(
        path=str(path),
        sample_rate=sr,
        hop_length=hop_length,
        feature_profile=feature_profile,
        channel_count=original_channel_count,
        mel_stream_names=mel_stream_names,
        chroma_dim=int(chroma.shape[1]),
        onset_band_names=onset_band_names,
        frame_times_seconds=frame_times.tolist(),
        mel_db=mel_db.tolist(),
        chroma=chroma.tolist(),
        onset_strength_values=onset_env.tolist(),
        onset_band_strengths=onset_bands.tolist(),
    )


def strength_at_time(analysis: AudioAnalysis, time_seconds: float) -> float:
    if not analysis.onset_strength_values:
        return 0.0
    if len(analysis.onset_strength_values) == 1:
        return analysis.onset_strength_values[0]

    clamped = min(max(time_seconds, 0.0), analysis.duration_seconds)
    times = analysis.onset_strength_times
    if clamped <= times[0]:
        return analysis.onset_strength_values[0]
    if clamped >= times[-1]:
        return analysis.onset_strength_values[-1]

    left = 0
    right = len(times) - 1
    while left + 1 < right:
        mid = (left + right) // 2
        if times[mid] <= clamped:
            left = mid
        else:
            right = mid

    t0 = times[left]
    t1 = times[right]
    v0 = analysis.onset_strength_values[left]
    v1 = analysis.onset_strength_values[right]
    if t1 <= t0:
        return v0
    mix = (clamped - t0) / (t1 - t0)
    return float(v0 + (v1 - v0) * mix)


def nearest_frame_index(frame_times_seconds: list[float], time_seconds: float) -> int:
    if not frame_times_seconds:
        return 0
    if time_seconds <= frame_times_seconds[0]:
        return 0
    if time_seconds >= frame_times_seconds[-1]:
        return len(frame_times_seconds) - 1

    left = 0
    right = len(frame_times_seconds) - 1
    while left + 1 < right:
        mid = (left + right) // 2
        if frame_times_seconds[mid] <= time_seconds:
            left = mid
        else:
            right = mid

    if abs(frame_times_seconds[left] - time_seconds) <= abs(frame_times_seconds[right] - time_seconds):
        return left
    return right
