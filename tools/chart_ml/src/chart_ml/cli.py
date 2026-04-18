from __future__ import annotations

import argparse
import json
from pathlib import Path

from chart_ml.audio_features import FEATURE_PROFILE_FULL, FEATURE_PROFILE_LEGACY, extract_frame_features
from chart_ml.corpus import build_corpus_manifest, summarize_manifest, write_corpus_manifest, write_corpus_report, write_split_jsonl
from chart_ml.dataset import dataset_stats, extract_chart_dataset, extract_window_dataset, write_jsonl
from chart_ml.external_4k import convert_chart_bundle_sets_to_song_packages, convert_direct_chart_sets_to_song_packages
from chart_ml.models import SongMeta
from chart_ml.rchart import parse_chart, write_song_package
from chart_ml.simple_model import generate_chart_from_template, train_simple_model


def build_parser() -> argparse.ArgumentParser:
    feature_profile_choices = (FEATURE_PROFILE_LEGACY, FEATURE_PROFILE_FULL)
    timing_mode_choices = ("fixed", "adaptive")
    parser = argparse.ArgumentParser(prog="chart-ml", description="Dataset tools for raythm ML experiments")
    subparsers = parser.add_subparsers(dest="command", required=True)

    charts_parser = subparsers.add_parser("extract-charts", help="Export one JSONL record per chart")
    charts_parser.add_argument("--songs-dir", required=True)
    charts_parser.add_argument("--output", required=True)
    charts_parser.add_argument("--key-count", type=int, default=None)
    charts_parser.add_argument("--manifest", default=None)
    charts_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    windows_parser = subparsers.add_parser("extract-windows", help="Export 4-event pattern windows for 4-key charts")
    windows_parser.add_argument("--songs-dir", required=True)
    windows_parser.add_argument("--output", required=True)
    windows_parser.add_argument("--key-count", type=int, default=4)
    windows_parser.add_argument("--span", type=int, default=4)
    windows_parser.add_argument("--manifest", default=None)
    windows_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    stats_parser = subparsers.add_parser("stats", help="Show dataset statistics")
    stats_parser.add_argument("--songs-dir", required=True)
    stats_parser.add_argument("--key-count", type=int, default=None)
    stats_parser.add_argument("--manifest", default=None)
    stats_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    import_external_parser = subparsers.add_parser("import-external-4k", help="Convert external 4-key chart data into raythm song packages")
    import_external_parser.add_argument("--source-root", required=True)
    import_external_parser.add_argument("--output-songs-dir", required=True)
    import_external_parser.add_argument("--resolution", type=int, default=480)

    build_corpus_parser = subparsers.add_parser("build-corpus", help="Build a reproducible corpus manifest, quality report, and fixed splits")
    build_corpus_parser.add_argument("--songs-dir", required=True)
    build_corpus_parser.add_argument("--output-manifest", required=True)
    build_corpus_parser.add_argument("--output-split-dir", default=None)
    build_corpus_parser.add_argument("--output-report", default=None)
    build_corpus_parser.add_argument("--key-count", type=int, default=4)
    build_corpus_parser.add_argument("--train-ratio", type=float, default=0.8)
    build_corpus_parser.add_argument("--validation-ratio", type=float, default=0.1)
    build_corpus_parser.add_argument("--test-ratio", type=float, default=0.1)
    build_corpus_parser.add_argument("--min-note-count", type=int, default=None)
    build_corpus_parser.add_argument("--max-note-count", type=int, default=None)
    build_corpus_parser.add_argument("--min-ln-ratio", type=float, default=None)
    build_corpus_parser.add_argument("--max-ln-ratio", type=float, default=None)
    build_corpus_parser.add_argument("--min-duration-seconds", type=float, default=None)
    build_corpus_parser.add_argument("--max-duration-seconds", type=float, default=None)
    build_corpus_parser.add_argument("--min-notes-per-second", type=float, default=None)
    build_corpus_parser.add_argument("--max-notes-per-second", type=float, default=None)
    build_corpus_parser.add_argument("--max-charts-per-song-family", type=int, default=None)
    build_corpus_parser.add_argument("--max-charts-per-chart-family", type=int, default=None)
    build_corpus_parser.add_argument("--keep-duplicates", action="store_true")

    analyze_audio_parser = subparsers.add_parser("analyze-audio", help="Analyze audio and estimate a timing map")
    analyze_audio_parser.add_argument("--audio", required=True)
    analyze_audio_parser.add_argument("--level", type=float, default=5.0)
    analyze_audio_parser.add_argument("--bpm", type=float, default=None)
    analyze_audio_parser.add_argument("--timing-mode", choices=timing_mode_choices, default="adaptive")
    analyze_audio_parser.add_argument("--feature-profile", choices=feature_profile_choices, default=FEATURE_PROFILE_FULL)

    evaluate_timing_parser = subparsers.add_parser("evaluate-timing", help="Compare generated timing candidates against an existing chart")
    evaluate_timing_parser.add_argument("--audio", required=True)
    evaluate_timing_parser.add_argument("--chart", required=True)
    evaluate_timing_parser.add_argument("--level", type=float, default=5.0)
    evaluate_timing_parser.add_argument("--bpm", type=float, default=None)
    evaluate_timing_parser.add_argument("--timing-mode", choices=timing_mode_choices, default="adaptive")

    train_simple_parser = subparsers.add_parser("train-simple", help="Train a simple transition model from 4K rchart data")
    train_simple_parser.add_argument("--songs-dir", required=True)
    train_simple_parser.add_argument("--output-model", required=True)
    train_simple_parser.add_argument("--subdivision", type=int, default=4)
    train_simple_parser.add_argument("--manifest", default=None)
    train_simple_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    prepare_neural_cache_parser = subparsers.add_parser("prepare-neural-cache", help="Precompute cached audio/chart features for neural training")
    prepare_neural_cache_parser.add_argument("--songs-dir", required=True)
    prepare_neural_cache_parser.add_argument("--cache-dir", required=True)
    prepare_neural_cache_parser.add_argument("--subdivision", type=int, default=8)
    prepare_neural_cache_parser.add_argument("--feature-profile", choices=feature_profile_choices, default=FEATURE_PROFILE_FULL)
    prepare_neural_cache_parser.add_argument("--force-rebuild", action="store_true")
    prepare_neural_cache_parser.add_argument("--song-id", default=None)
    prepare_neural_cache_parser.add_argument("--chart-id", default=None)
    prepare_neural_cache_parser.add_argument("--min-level", type=float, default=None)
    prepare_neural_cache_parser.add_argument("--max-level", type=float, default=None)
    prepare_neural_cache_parser.add_argument("--limit", type=int, default=None)
    prepare_neural_cache_parser.add_argument("--manifest", default=None)
    prepare_neural_cache_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    train_neural_parser = subparsers.add_parser("train-neural", help="Train a neural chart model from audio + 4K charts")
    train_neural_parser.add_argument("--songs-dir", required=True)
    train_neural_parser.add_argument("--output-model", required=True)
    train_neural_parser.add_argument("--subdivision", type=int, default=8)
    train_neural_parser.add_argument("--feature-profile", choices=feature_profile_choices, default=FEATURE_PROFILE_FULL)
    train_neural_parser.add_argument("--chunk-length", type=int, default=256)
    train_neural_parser.add_argument("--epochs", type=int, default=6)
    train_neural_parser.add_argument("--batch-size", type=int, default=4)
    train_neural_parser.add_argument("--learning-rate", type=float, default=1e-3)
    train_neural_parser.add_argument("--device", default="cpu")
    train_neural_parser.add_argument("--cache-dir", default=None)
    train_neural_parser.add_argument("--force-rebuild-cache", action="store_true")
    train_neural_parser.add_argument("--song-id", default=None)
    train_neural_parser.add_argument("--chart-id", default=None)
    train_neural_parser.add_argument("--min-level", type=float, default=None)
    train_neural_parser.add_argument("--max-level", type=float, default=None)
    train_neural_parser.add_argument("--limit", type=int, default=None)
    train_neural_parser.add_argument("--validation-ratio", type=float, default=0.15)
    train_neural_parser.add_argument("--manifest", default=None)
    train_neural_parser.add_argument("--split", choices=("train", "validation", "test"), default=None)

    generate_simple_parser = subparsers.add_parser("generate-simple", help="Generate one rchart from the simple model")
    generate_simple_parser.add_argument("--model", required=True)
    generate_simple_parser.add_argument("--template-chart", required=True)
    generate_simple_parser.add_argument("--output", required=True)
    generate_simple_parser.add_argument("--song-id", required=True)
    generate_simple_parser.add_argument("--chart-id", required=True)
    generate_simple_parser.add_argument("--difficulty", default="ML_SIMPLE")
    generate_simple_parser.add_argument("--seed", type=int, default=0)
    generate_simple_parser.add_argument("--chart-author", default="chart_ml simple model")

    generate_audio_parser = subparsers.add_parser("generate-audio", help="Generate a chart from audio using a fixed-BPM timing map")
    generate_audio_parser.add_argument("--model", required=True)
    generate_audio_parser.add_argument("--audio", required=True)
    generate_audio_parser.add_argument("--output", required=True)
    generate_audio_parser.add_argument("--song-id", required=True)
    generate_audio_parser.add_argument("--chart-id", required=True)
    generate_audio_parser.add_argument("--difficulty", default="ML_AUDIO")
    generate_audio_parser.add_argument("--level", type=float, default=5.0)
    generate_audio_parser.add_argument("--bpm", type=float, default=None)
    generate_audio_parser.add_argument("--timing-mode", choices=timing_mode_choices, default="adaptive")
    generate_audio_parser.add_argument("--seed", type=int, default=0)
    generate_audio_parser.add_argument("--chart-author", default="chart_ml audio model")
    generate_audio_parser.add_argument("--output-song-dir", default=None)
    generate_audio_parser.add_argument("--title", default=None)
    generate_audio_parser.add_argument("--artist", default="Unknown")
    generate_audio_parser.add_argument("--preview-start-seconds", type=float, default=0.0)
    generate_audio_parser.add_argument("--jacket-source", default=None)

    generate_neural_parser = subparsers.add_parser("generate-neural", help="Generate a chart from audio with the neural model")
    generate_neural_parser.add_argument("--model", required=True)
    generate_neural_parser.add_argument("--audio", required=True)
    generate_neural_parser.add_argument("--output", required=True)
    generate_neural_parser.add_argument("--song-id", required=True)
    generate_neural_parser.add_argument("--chart-id", required=True)
    generate_neural_parser.add_argument("--difficulty", default="ML_NEURAL")
    generate_neural_parser.add_argument("--feature-profile", choices=feature_profile_choices, default=FEATURE_PROFILE_FULL)
    generate_neural_parser.add_argument("--level", type=float, default=8.0)
    generate_neural_parser.add_argument("--bpm", type=float, default=None)
    generate_neural_parser.add_argument("--timing-mode", choices=timing_mode_choices, default="adaptive")
    generate_neural_parser.add_argument("--template-chart", default=None)
    generate_neural_parser.add_argument("--chart-author", default="chart_ml neural model")
    generate_neural_parser.add_argument("--output-song-dir", default=None)
    generate_neural_parser.add_argument("--title", default=None)
    generate_neural_parser.add_argument("--artist", default="Unknown")
    generate_neural_parser.add_argument("--preview-start-seconds", type=float, default=0.0)
    generate_neural_parser.add_argument("--jacket-source", default=None)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.command == "extract-charts":
        records = extract_chart_dataset(
            args.songs_dir,
            key_count=args.key_count,
            manifest_path=args.manifest,
            split=args.split,
        )
        write_jsonl(records, args.output)
        print(f"wrote {len(records)} chart records to {args.output}")
        return 0

    if args.command == "extract-windows":
        records = extract_window_dataset(
            args.songs_dir,
            key_count=args.key_count,
            span=args.span,
            manifest_path=args.manifest,
            split=args.split,
        )
        write_jsonl(records, args.output)
        print(f"wrote {len(records)} window records to {args.output}")
        return 0

    if args.command == "stats":
        print(json.dumps(
            dataset_stats(
                args.songs_dir,
                key_count=args.key_count,
                manifest_path=args.manifest,
                split=args.split,
            ),
            indent=2,
            ensure_ascii=True,
        ))
        return 0

    if args.command == "import-external-4k":
        converted = convert_direct_chart_sets_to_song_packages(args.source_root, args.output_songs_dir, resolution=args.resolution)
        if converted == 0:
            converted = convert_chart_bundle_sets_to_song_packages(args.source_root, args.output_songs_dir, resolution=args.resolution)
        print(f"converted {converted} charts into {args.output_songs_dir}")
        return 0

    if args.command == "build-corpus":
        manifest = build_corpus_manifest(
            songs_dir=args.songs_dir,
            key_count=args.key_count,
            train_ratio=args.train_ratio,
            validation_ratio=args.validation_ratio,
            test_ratio=args.test_ratio,
            min_note_count=args.min_note_count,
            max_note_count=args.max_note_count,
            min_ln_ratio=args.min_ln_ratio,
            max_ln_ratio=args.max_ln_ratio,
            min_duration_seconds=args.min_duration_seconds,
            max_duration_seconds=args.max_duration_seconds,
            min_notes_per_second=args.min_notes_per_second,
            max_notes_per_second=args.max_notes_per_second,
            exclude_duplicates=not args.keep_duplicates,
            max_charts_per_song_family=args.max_charts_per_song_family,
            max_charts_per_chart_family=args.max_charts_per_chart_family,
        )
        write_corpus_manifest(manifest, args.output_manifest)
        if args.output_split_dir:
            write_split_jsonl(manifest, args.output_split_dir)
        if args.output_report:
            write_corpus_report(manifest, args.output_report)
        summary = summarize_manifest(manifest, manifest_path=args.output_manifest)
        print(json.dumps({
            "manifest_path": summary.manifest_path,
            "song_count": summary.total_song_count,
            "chart_count": summary.total_chart_count,
            "included_chart_count": summary.included_chart_count,
            "excluded_chart_count": summary.excluded_chart_count,
            "split_counts": summary.split_counts,
            "exclusion_reasons": summary.exclusion_reasons,
            "output_report": args.output_report,
        }, indent=2, ensure_ascii=True))
        return 0

    if args.command == "analyze-audio":
        try:
            from chart_ml.audio_features import analyze_audio
            from chart_ml.timing import generate_timing
        except ModuleNotFoundError as exc:
            parser.exit(1, f"audio dependencies are missing ({exc}). Install with: pip install -e .\n")

        try:
            analysis = analyze_audio(args.audio)
            frame_features = extract_frame_features(args.audio, feature_profile=args.feature_profile)
            timing = generate_timing(analysis=analysis, level=args.level, bpm_override=args.bpm, timing_mode=args.timing_mode)
        except ModuleNotFoundError as exc:
            parser.exit(1, f"audio dependencies are missing ({exc}). Install with: pip install -e .\n")
        summary = {
            "audio": args.audio,
            "feature_profile": args.feature_profile,
            "timing_mode": timing.timing_mode,
            "channel_count": analysis.channel_count,
            "duration_seconds": analysis.duration_seconds,
            "active_start_seconds": analysis.active_start_seconds,
            "active_end_seconds": analysis.active_end_seconds,
            "estimated_bpm": analysis.tempo_bpm,
            "used_bpm": timing.bpm,
            "beat_count": len(analysis.beat_times),
            "onset_count": len(analysis.onset_times),
            "generated_beat_count": len(timing.beat_ticks),
            "measure_count": len(timing.measure_start_ticks),
            "tempo_segment_count": len(timing.tempo_segments),
            "bpm_min": min((segment.bpm for segment in timing.tempo_segments), default=timing.bpm),
            "bpm_max": max((segment.bpm for segment in timing.tempo_segments), default=timing.bpm),
            "mel_streams": frame_features.mel_stream_names,
            "mel_feature_dim": len(frame_features.mel_db[0]) if frame_features.mel_db else 0,
            "chroma_dim": frame_features.chroma_dim,
            "onset_band_names": frame_features.onset_band_names,
            "event_count": len(timing.event_ticks),
            "subdivision": timing.subdivision,
        }
        print(json.dumps(summary, indent=2, ensure_ascii=True))
        return 0

    if args.command == "evaluate-timing":
        try:
            from chart_ml.audio_features import analyze_audio
            from chart_ml.timing import evaluate_timing_against_chart, generate_timing
        except ModuleNotFoundError as exc:
            parser.exit(1, f"audio dependencies are missing ({exc}). Install with: pip install -e .\n")

        analysis = analyze_audio(args.audio)
        timing = generate_timing(
            analysis=analysis,
            level=args.level,
            bpm_override=args.bpm,
            timing_mode=args.timing_mode,
        )
        chart = parse_chart(args.chart)
        evaluation = evaluate_timing_against_chart(timing, chart)
        print(json.dumps({
            "audio": args.audio,
            "chart": args.chart,
            "timing_mode": evaluation.timing_mode,
            "active_start_seconds": analysis.active_start_seconds,
            "active_end_seconds": analysis.active_end_seconds,
            "generated_event_count": evaluation.generated_event_count,
            "generated_beat_count": evaluation.generated_beat_count,
            "chart_beat_count": evaluation.chart_beat_count,
            "note_count": evaluation.note_count,
            "note_mean_abs_error_ms": evaluation.note_mean_abs_error_ms,
            "note_p90_abs_error_ms": evaluation.note_p90_abs_error_ms,
            "note_hit_rate_30ms": evaluation.note_hit_rate_30ms,
            "note_hit_rate_60ms": evaluation.note_hit_rate_60ms,
            "note_hit_rate_90ms": evaluation.note_hit_rate_90ms,
            "beat_mean_abs_error_ms": evaluation.beat_mean_abs_error_ms,
            "beat_p90_abs_error_ms": evaluation.beat_p90_abs_error_ms,
            "beat_hit_rate_30ms": evaluation.beat_hit_rate_30ms,
            "beat_hit_rate_60ms": evaluation.beat_hit_rate_60ms,
            "tempo_segment_count": evaluation.tempo_segment_count,
            "bpm_min": evaluation.bpm_min,
            "bpm_max": evaluation.bpm_max,
            "bpm_median": evaluation.bpm_median,
        }, indent=2, ensure_ascii=True))
        return 0

    if args.command == "train-simple":
        model = train_simple_model(
            args.songs_dir,
            key_count=4,
            subdivision=args.subdivision,
            manifest_path=args.manifest,
            split=args.split,
        )
        model.save(args.output_model)
        print(f"saved simple model to {args.output_model}")
        return 0

    if args.command == "prepare-neural-cache":
        try:
            from chart_ml.deep_dataset import prepare_sequence_cache
        except ModuleNotFoundError as exc:
            parser.exit(1, f"training dependencies are missing ({exc}). Install with: pip install -e .[train]\n")

        summary = prepare_sequence_cache(
            songs_dir=args.songs_dir,
            cache_dir=args.cache_dir,
            subdivision=args.subdivision,
            force_rebuild=args.force_rebuild,
            song_id=args.song_id,
            chart_id=args.chart_id,
            min_level=args.min_level,
            max_level=args.max_level,
            limit=args.limit,
            manifest_path=args.manifest,
            split=args.split,
            feature_profile=args.feature_profile,
        )
        print(json.dumps({
            "songs_dir": args.songs_dir,
            "cache_dir": args.cache_dir,
            "feature_profile": args.feature_profile,
            "example_count": summary.example_count,
            "cache_hits": summary.cache_hits,
            "cache_misses": summary.cache_misses,
        }, indent=2, ensure_ascii=True))
        return 0

    if args.command == "train-neural":
        try:
            from chart_ml.deep_training import train_neural_chart_model
        except ModuleNotFoundError as exc:
            parser.exit(1, f"training dependencies are missing ({exc}). Install with: pip install -e .[train]\n")

        summary = train_neural_chart_model(
            songs_dir=args.songs_dir,
            output_model=args.output_model,
            subdivision=args.subdivision,
            chunk_length=args.chunk_length,
            epochs=args.epochs,
            batch_size=args.batch_size,
            learning_rate=args.learning_rate,
            device_name=args.device,
            cache_dir=args.cache_dir,
            force_rebuild_cache=args.force_rebuild_cache,
            song_id=args.song_id,
            chart_id=args.chart_id,
            min_level=args.min_level,
            max_level=args.max_level,
            limit=args.limit,
            validation_ratio=args.validation_ratio,
            manifest_path=args.manifest,
            split=args.split,
            feature_profile=args.feature_profile,
        )
        print(json.dumps({
            "output_model": args.output_model,
            "feature_profile": args.feature_profile,
            "train_examples": summary.train_examples,
            "validation_examples": summary.validation_examples,
            "train_chunks": summary.train_chunks,
            "validation_chunks": summary.validation_chunks,
            "best_validation_loss": summary.best_validation_loss,
            "last_train_loss": summary.last_train_loss,
            "epochs": summary.epochs,
            "cache_dir": summary.cache_dir,
        }, indent=2, ensure_ascii=True))
        return 0

    if args.command == "generate-simple":
        from chart_ml.simple_model import SimplePatternModel

        model = SimplePatternModel.load(args.model)
        chart = generate_chart_from_template(
            model=model,
            template_chart_path=args.template_chart,
            output_path=args.output,
            song_id=args.song_id,
            chart_id=args.chart_id,
            difficulty=args.difficulty,
            seed=args.seed,
            chart_author=args.chart_author,
        )
        print(f"generated {len(chart.notes)} notes to {args.output}")
        return 0

    if args.command == "generate-audio":
        try:
            from chart_ml.simple_model import SimplePatternModel
            from chart_ml.simple_model import generate_chart_from_audio
        except ModuleNotFoundError as exc:
            parser.exit(1, f"audio dependencies are missing ({exc}). Install with: pip install -e .\n")

        model = SimplePatternModel.load(args.model)
        try:
            chart = generate_chart_from_audio(
                model=model,
                audio_path=args.audio,
                output_path=args.output,
                song_id=args.song_id,
                chart_id=args.chart_id,
                difficulty=args.difficulty,
                level=args.level,
                bpm_override=args.bpm,
                timing_mode=args.timing_mode,
                seed=args.seed,
                chart_author=args.chart_author,
            )
        except ModuleNotFoundError as exc:
            parser.exit(1, f"audio dependencies are missing ({exc}). Install with: pip install -e .\n")

        if args.output_song_dir:
            bpm_events = [event for event in chart.timing_events if event.event_type == "bpm" and event.bpm is not None]
            base_bpm = bpm_events[0].bpm if bpm_events else (args.bpm if args.bpm is not None else 120.0)
            song_meta = SongMeta(
                song_id=args.song_id,
                title=args.title or Path(args.audio).stem,
                artist=args.artist,
                base_bpm=float(base_bpm),
                audio_file=Path(args.audio).name,
                jacket_file=Path(args.jacket_source).name if args.jacket_source else "",
                preview_start_seconds=args.preview_start_seconds,
                song_version=1,
            )
            _, chart_path = write_song_package(
                song_meta=song_meta,
                chart=chart,
                song_dir=args.output_song_dir,
                source_audio_path=args.audio,
                source_jacket_path=args.jacket_source,
                chart_file_name=f"{args.chart_id}.rchart",
            )
            print(f"generated {len(chart.notes)} notes to {args.output} and packaged chart to {chart_path}")
            return 0

        print(f"generated {len(chart.notes)} notes to {args.output}")
        return 0

    if args.command == "generate-neural":
        try:
            from chart_ml.deep_dataset import build_chart_step_grid, build_inference_features
            from chart_ml.deep_model import generate_chart_with_neural_model, load_trained_model
            from chart_ml.timing import generate_timing
            from chart_ml.audio_features import analyze_audio
        except ModuleNotFoundError as exc:
            parser.exit(1, f"neural generation dependencies are missing ({exc}). Install with: pip install -e .[train]\n")

        model, config = load_trained_model(args.model)
        feature_profile = config.feature_profile or args.feature_profile
        if args.template_chart:
            template_chart = parse_chart(args.template_chart)
            bpm_events = [event for event in template_chart.timing_events if event.event_type == "bpm" and event.bpm is not None]
            inferred_bpm = bpm_events[0].bpm if bpm_events else (args.bpm if args.bpm is not None else 120.0)
            step_ticks, step_times_seconds = build_chart_step_grid(template_chart, inferred_bpm, config.subdivision)
            class TemplateTiming:
                resolution = template_chart.meta.resolution
                subdivision = config.subdivision
                bpm = float(inferred_bpm)
                event_ticks = step_ticks
                event_times_seconds = step_times_seconds
                timing_events = template_chart.timing_events
            timing = TemplateTiming()
        else:
            analysis = analyze_audio(args.audio, sample_rate=config.sample_rate)
            timing = generate_timing(
                analysis=analysis,
                level=args.level,
                resolution=480,
                bpm_override=args.bpm,
                timing_mode=args.timing_mode,
            )
        feature_steps = build_inference_features(
            audio_path=args.audio,
            step_times_seconds=timing.event_times_seconds,
            step_ticks=timing.event_ticks,
            resolution=timing.resolution,
            subdivision=timing.subdivision,
            level=args.level,
            bpm=timing.bpm,
            sample_rate=config.sample_rate,
            hop_length=config.hop_length,
            n_mels=config.n_mels,
            feature_profile=feature_profile,
        )
        chart = generate_chart_with_neural_model(
            model=model,
            config=config,
            feature_steps=feature_steps,
            step_ticks=timing.event_ticks,
            timing_events=timing.timing_events,
            output_path=args.output,
            song_id=args.song_id,
            chart_id=args.chart_id,
            difficulty=args.difficulty,
            level=args.level,
            chart_author=args.chart_author,
            resolution=timing.resolution,
        )
        if args.output_song_dir:
            bpm_events = [event for event in chart.timing_events if event.event_type == "bpm" and event.bpm is not None]
            base_bpm = bpm_events[0].bpm if bpm_events else (args.bpm if args.bpm is not None else 120.0)
            song_meta = SongMeta(
                song_id=args.song_id,
                title=args.title or Path(args.audio).stem,
                artist=args.artist,
                base_bpm=float(base_bpm),
                audio_file=Path(args.audio).name,
                jacket_file=Path(args.jacket_source).name if args.jacket_source else "",
                preview_start_seconds=args.preview_start_seconds,
                song_version=1,
            )
            _, chart_path = write_song_package(
                song_meta=song_meta,
                chart=chart,
                song_dir=args.output_song_dir,
                source_audio_path=args.audio,
                source_jacket_path=args.jacket_source,
                chart_file_name=f"{args.chart_id}.rchart",
            )
            print(f"generated {len(chart.notes)} notes to {args.output} and packaged chart to {chart_path}")
            return 0

        print(f"generated {len(chart.notes)} notes to {args.output}")
        return 0

    parser.error(f"unknown command: {args.command}")
    return 2
