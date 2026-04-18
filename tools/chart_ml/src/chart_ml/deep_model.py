from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path

from chart_ml.audio_features import FEATURE_PROFILE_FULL, feature_layout
from chart_ml.deep_dataset import INDEX_TO_STATE
from chart_ml.models import Chart, ChartMeta, Note, TimingEvent
from chart_ml.rchart import write_chart


@dataclass(slots=True)
class NeuralModelConfig:
    input_dim: int
    hidden_dim: int = 160
    num_layers: int = 2
    dropout: float = 0.2
    feature_profile: str = FEATURE_PROFILE_FULL
    subdivision: int = 8
    n_mels: int = 64
    mel_stream_count: int = 4
    chroma_dim: int = 12
    band_onset_count: int = 3
    hop_length: int = 512
    sample_rate: int = 22050

    @property
    def overall_onset_index(self) -> int:
        layout = feature_layout(
            feature_profile=self.feature_profile,
            n_mels=self.n_mels,
            chroma_bins=self.chroma_dim,
            band_onset_count=self.band_onset_count,
        )
        return int(layout["overall_onset_index"])


def build_chart_generation_model(config: NeuralModelConfig):
    import torch
    import torch.nn as nn

    class ChartGenerationNet(nn.Module):
        def __init__(self) -> None:
            super().__init__()
            self.input_projection = nn.Linear(config.input_dim, config.hidden_dim)
            self.encoder = nn.GRU(
                input_size=config.hidden_dim,
                hidden_size=config.hidden_dim,
                num_layers=config.num_layers,
                batch_first=True,
                dropout=config.dropout if config.num_layers > 1 else 0.0,
                bidirectional=True,
            )
            self.output = nn.Linear(config.hidden_dim * 2, 4 * len(INDEX_TO_STATE))

        def forward(self, features, lengths):
            projected = torch.relu(self.input_projection(features))
            packed = nn.utils.rnn.pack_padded_sequence(projected, lengths.cpu(), batch_first=True, enforce_sorted=False)
            encoded, _ = self.encoder(packed)
            unpacked, _ = nn.utils.rnn.pad_packed_sequence(encoded, batch_first=True)
            logits = self.output(unpacked)
            return logits.view(logits.shape[0], logits.shape[1], 4, len(INDEX_TO_STATE))

    return ChartGenerationNet()


def save_trained_model(model, config: NeuralModelConfig, path: str | Path) -> None:
    import torch

    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    torch.save({"config": asdict(config), "state_dict": model.state_dict()}, target)


def load_trained_model(path: str | Path):
    import torch

    payload = torch.load(Path(path), map_location="cpu")
    config = NeuralModelConfig(**payload["config"])
    model = build_chart_generation_model(config)
    model.load_state_dict(payload["state_dict"])
    model.eval()
    return model, config


def _lane_states_from_logits(logits) -> list[list[str]]:
    import torch

    predicted = torch.argmax(logits, dim=-1).cpu().tolist()
    return [[INDEX_TO_STATE[int(state)] for state in step] for step in predicted]


def _decode_lane_actions_viterbi(logits, feature_steps, config: NeuralModelConfig) -> list[list[str]]:
    import math
    import torch

    action_names = [INDEX_TO_STATE[index] for index in range(len(INDEX_TO_STATE))]
    log_probs = torch.log_softmax(logits, dim=-1).cpu().tolist()
    features = feature_steps.detach().cpu().tolist() if hasattr(feature_steps, "detach") else feature_steps.tolist()
    time_steps = len(log_probs)
    lane_count = len(log_probs[0]) if log_probs else 4

    decoded = [["off" for _ in range(lane_count)] for _ in range(time_steps)]
    for lane in range(lane_count):
        dp = [{False: -math.inf, True: -math.inf} for _ in range(time_steps)]
        back: list[dict[bool, tuple[bool, str] | None]] = [{False: None, True: None} for _ in range(time_steps)]
        dp[0][False] = 0.0

        for step in range(time_steps):
            next_dp = {False: -math.inf, True: -math.inf}
            next_back: dict[bool, tuple[bool, str] | None] = {False: None, True: None}
            for active in (False, True):
                base_score = dp[step - 1][active] if step > 0 else (0.0 if not active else -math.inf)
                if base_score == -math.inf:
                    continue

                for action_index, action_name in enumerate(action_names):
                    next_active = active
                    if not active:
                        if action_name == "hold_end":
                            continue
                        if action_name == "hold_start":
                            next_active = True
                    else:
                        if action_name in {"tap", "hold_start"}:
                            continue
                        if action_name == "hold_end":
                            next_active = False
                        else:
                            next_active = True

                    onset_value = float(features[step][config.overall_onset_index]) if features else 0.0
                    onset_clamped = max(-1.5, min(2.0, onset_value))
                    action_penalty = 0.0
                    if action_name == "tap":
                        action_penalty = -0.95 + 0.28 * onset_clamped
                    elif action_name == "hold_start":
                        action_penalty = -1.10 + 0.32 * onset_clamped
                    elif action_name == "hold_end":
                        action_penalty = -0.06

                    score = base_score + float(log_probs[step][lane][action_index]) + action_penalty
                    if score > next_dp[next_active]:
                        next_dp[next_active] = score
                        next_back[next_active] = (active, action_name)

            dp[step] = next_dp
            back[step] = next_back

        active = False if dp[-1][False] >= dp[-1][True] else True
        for step in range(time_steps - 1, -1, -1):
            previous = back[step][active]
            if previous is None:
                decoded[step][lane] = "off"
                active = False
                continue
            previous_active, action_name = previous
            decoded[step][lane] = action_name
            active = previous_active

        lane_active = False
        for step in range(time_steps):
            action_name = decoded[step][lane]
            if lane_active:
                if action_name == "hold_end":
                    lane_active = False
                elif action_name != "off":
                    decoded[step][lane] = "off"
            else:
                if action_name == "hold_start":
                    lane_active = True
                elif action_name == "hold_end":
                    decoded[step][lane] = "off"

        if lane_active and time_steps > 0:
            decoded[-1][lane] = "hold_end"

    return decoded


def lane_states_to_chart(
    lane_states_by_step: list[list[str]],
    step_ticks: list[int],
    timing_events: list[TimingEvent],
    chart_id: str,
    song_id: str,
    difficulty: str,
    level: float,
    resolution: int,
    chart_author: str,
) -> Chart:
    notes: list[Note] = []
    active_holds: dict[int, int] = {}

    for tick, step_states in zip(step_ticks, lane_states_by_step):
        for lane, state in enumerate(step_states):
            if state == "tap":
                notes.append(Note(note_type="tap", tick=tick, lane=lane))
            elif state == "hold_start":
                active_holds[lane] = tick
            elif state == "hold_end":
                start_tick = active_holds.pop(lane, None)
                if start_tick is not None and tick > start_tick:
                    notes.append(Note(note_type="hold", tick=start_tick, lane=lane, end_tick=tick))

    final_tick = step_ticks[-1] if step_ticks else 0
    for lane, start_tick in list(active_holds.items()):
        if final_tick > start_tick:
            notes.append(Note(note_type="hold", tick=start_tick, lane=lane, end_tick=final_tick))

    notes.sort(key=lambda note: (note.tick, note.lane, note.end_tick or note.tick))
    return Chart(
        path="",
        meta=ChartMeta(
            chart_id=chart_id,
            key_count=4,
            difficulty=difficulty,
            level=level,
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


def generate_chart_with_neural_model(
    model,
    config: NeuralModelConfig,
    feature_steps,
    step_ticks: list[int],
    timing_events: list[TimingEvent],
    output_path: str | Path,
    song_id: str,
    chart_id: str,
    difficulty: str,
    level: float,
    chart_author: str,
    resolution: int,
) -> Chart:
    import torch

    device = next(model.parameters()).device
    features = torch.tensor(feature_steps, dtype=torch.float32, device=device).unsqueeze(0)
    lengths = torch.tensor([feature_steps.shape[0]], dtype=torch.long, device=device)
    with torch.no_grad():
        logits = model(features, lengths)[0]
    lane_states = _decode_lane_actions_viterbi(logits, features[0], config)
    chart = lane_states_to_chart(
        lane_states_by_step=lane_states,
        step_ticks=step_ticks,
        timing_events=timing_events,
        chart_id=chart_id,
        song_id=song_id,
        difficulty=difficulty,
        level=level,
        resolution=resolution,
        chart_author=chart_author,
    )
    write_chart(chart, output_path)
    return chart
