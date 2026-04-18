from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from chart_ml.audio_features import FEATURE_PROFILE_FULL, FEATURE_PROFILE_LEGACY
from chart_ml.deep_dataset import ChunkedSequenceDataset, collate_chunk_batch, load_sequence_examples, split_train_validation
from chart_ml.deep_model import NeuralModelConfig, build_chart_generation_model, save_trained_model


@dataclass(slots=True)
class TrainingSummary:
    train_examples: int
    validation_examples: int
    train_chunks: int
    validation_chunks: int
    best_validation_loss: float
    last_train_loss: float
    epochs: int
    cache_dir: str | None = None


def _class_weights(examples) -> list[float]:
    counts = [1.0] * 4
    for example in examples:
        for state_index in example.labels.reshape(-1):
            counts[int(state_index)] += 1.0
    total = sum(counts)
    weights = [total / value for value in counts]
    scale = max(weights)
    return [float(weight / scale) for weight in weights]


def train_neural_chart_model(
    songs_dir: str | Path,
    output_model: str | Path,
    subdivision: int = 8,
    chunk_length: int = 256,
    epochs: int = 6,
    batch_size: int = 4,
    learning_rate: float = 1e-3,
    device_name: str = "cpu",
    cache_dir: str | Path | None = None,
    force_rebuild_cache: bool = False,
    song_id: str | None = None,
    chart_id: str | None = None,
    min_level: float | None = None,
    max_level: float | None = None,
    limit: int | None = None,
    validation_ratio: float = 0.15,
    manifest_path: str | Path | None = None,
    split: str | None = None,
    feature_profile: str = FEATURE_PROFILE_FULL,
) -> TrainingSummary:
    import torch
    from torch import nn
    from torch.utils.data import DataLoader

    examples = load_sequence_examples(
        songs_dir,
        subdivision=subdivision,
        cache_dir=cache_dir,
        force_rebuild=force_rebuild_cache,
        song_id=song_id,
        chart_id=chart_id,
        min_level=min_level,
        max_level=max_level,
        limit=limit,
        manifest_path=manifest_path,
        split=split,
        feature_profile=feature_profile,
    )
    train_examples, validation_examples = split_train_validation(examples, validation_ratio=validation_ratio)
    if not train_examples:
        raise ValueError("No training examples could be built from the provided songs directory.")

    train_dataset = ChunkedSequenceDataset(train_examples, chunk_length=chunk_length)
    validation_dataset = ChunkedSequenceDataset(validation_examples, chunk_length=chunk_length)

    feature_dim = int(train_examples[0].feature_steps.shape[-1])
    config = NeuralModelConfig(
        input_dim=feature_dim,
        feature_profile=feature_profile,
        subdivision=subdivision,
    )
    if feature_profile == FEATURE_PROFILE_LEGACY:
        config.mel_stream_count = 1
        config.chroma_dim = 0
        config.band_onset_count = 0
    else:
        config.mel_stream_count = 4
        config.chroma_dim = 12
        config.band_onset_count = 3
    model = build_chart_generation_model(config)
    device = torch.device(device_name)
    model.to(device)

    optimizer = torch.optim.AdamW(model.parameters(), lr=learning_rate)
    loss_fn = nn.CrossEntropyLoss(
        ignore_index=-100,
        weight=torch.tensor(_class_weights(train_examples), dtype=torch.float32, device=device),
    )

    train_loader = DataLoader(train_dataset, batch_size=batch_size, shuffle=True, collate_fn=collate_chunk_batch)
    validation_loader = DataLoader(validation_dataset, batch_size=batch_size, shuffle=False, collate_fn=collate_chunk_batch)

    best_validation_loss = float("inf")
    best_state = None
    last_train_loss = 0.0

    for _epoch in range(epochs):
        model.train()
        train_loss = 0.0
        train_batches = 0
        for batch in train_loader:
            features = batch["features"].to(device)
            labels = batch["labels"].to(device)
            lengths = batch["lengths"].to(device)

            optimizer.zero_grad(set_to_none=True)
            logits = model(features, lengths)
            loss = loss_fn(logits.reshape(-1, logits.shape[-1]), labels.reshape(-1))
            loss.backward()
            optimizer.step()
            train_loss += float(loss.item())
            train_batches += 1

        last_train_loss = train_loss / max(train_batches, 1)

        if validation_examples:
            model.eval()
            validation_loss = 0.0
            validation_batches = 0
            with torch.no_grad():
                for batch in validation_loader:
                    features = batch["features"].to(device)
                    labels = batch["labels"].to(device)
                    lengths = batch["lengths"].to(device)
                    logits = model(features, lengths)
                    loss = loss_fn(logits.reshape(-1, logits.shape[-1]), labels.reshape(-1))
                    validation_loss += float(loss.item())
                    validation_batches += 1

            average_validation_loss = validation_loss / max(validation_batches, 1)
            if average_validation_loss <= best_validation_loss:
                best_validation_loss = average_validation_loss
                best_state = {key: value.detach().cpu().clone() for key, value in model.state_dict().items()}
        else:
            best_validation_loss = last_train_loss
            best_state = {key: value.detach().cpu().clone() for key, value in model.state_dict().items()}

    if best_state is not None:
        model.load_state_dict(best_state)

    save_trained_model(model, config, output_model)
    return TrainingSummary(
        train_examples=len(train_examples),
        validation_examples=len(validation_examples),
        train_chunks=len(train_dataset),
        validation_chunks=len(validation_dataset),
        best_validation_loss=best_validation_loss,
        last_train_loss=last_train_loss,
        epochs=epochs,
        cache_dir=str(cache_dir) if cache_dir is not None else None,
    )
