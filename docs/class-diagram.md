# クラス図

## Phase 1-1: シーン管理システム

```mermaid
classDiagram
    class scene {
        <<abstract>>
        #scene_manager& manager_
        +update(float dt) void*
        +draw() void*
        +on_enter() void
        +on_exit() void
    }

    class scene_manager {
        -unique_ptr~scene~ current_scene_
        -unique_ptr~scene~ next_scene_
        +update(float dt) void
        +draw() void
        +change_scene(unique_ptr~scene~) void
    }

    class title_scene {
        +update(float dt) void
        +draw() void
        +on_enter() void
        +on_exit() void
    }

    class song_select_scene {
        +update(float dt) void
        +draw() void
        +on_enter() void
        +on_exit() void
    }

    class play_scene {
        +update(float dt) void
        +draw() void
        +on_enter() void
        +on_exit() void
    }

    class result_scene {
        +update(float dt) void
        +draw() void
        +on_enter() void
        +on_exit() void
    }

    class settings_scene {
        +update(float dt) void
        +draw() void
        +on_enter() void
        +on_exit() void
    }

    scene_manager o-- scene : manages
    scene --> scene_manager : requests transition

    scene <|-- title_scene
    scene <|-- song_select_scene
    scene <|-- play_scene
    scene <|-- result_scene
    scene <|-- settings_scene
```

## Phase 1-2: データモデル

```mermaid
classDiagram
    class song_meta {
        +string song_id
        +string title
        +string artist
        +float base_bpm
        +string audio_file
        +string jacket_file
        +int preview_start_ms
        +int song_version
    }

    class chart_meta {
        +string chart_id
        +int key_count
        +string difficulty
        +int level
        +string chart_author
        +int format_version
        +int resolution
    }

    class timing_event_type {
        <<enumeration>>
        bpm
        meter
    }

    class timing_event {
        +timing_event_type type
        +int tick
        +float bpm
        +int numerator
        +int denominator
    }

    class note_type {
        <<enumeration>>
        tap
        hold
    }

    class note_data {
        +note_type type
        +int tick
        +int lane
        +int end_tick
    }

    class judge_result {
        <<enumeration>>
        perfect
        great
        good
        bad
        miss
    }

    class rank {
        <<enumeration>>
        ss
        s
        a
        b
        c
        f
    }

    class result_data {
        +int score
        +float achievement
        +array~int, 5~ judge_counts
        +int max_combo
        +float avg_offset
        +int fast_count
        +int slow_count
        +rank rank
        +bool is_full_combo
        +bool is_all_perfect
    }

    timing_event --> timing_event_type
    note_data --> note_type
    result_data --> rank
    result_data --> judge_result
```

## Phase 2-1: 譜面パーサー

```mermaid
classDiagram
    class chart_parser {
        <<static>>
        +parse(string file_path) chart_parse_result
        -parse_metadata(vector~string~ lines) chart_meta
        -parse_timing(vector~string~ lines) vector~timing_event~
        -parse_notes(vector~string~ lines) vector~note_data~
        -validate(chart_data data) vector~string~
    }

    class chart_data {
        +chart_meta meta
        +vector~timing_event~ timing_events
        +vector~note_data~ notes
    }

    class chart_parse_result {
        +bool success
        +optional~chart_data~ data
        +vector~string~ errors
    }

    chart_parser --> chart_parse_result : returns
    chart_parse_result --> chart_data : contains
    chart_data --> chart_meta
    chart_data --> timing_event
    chart_data --> note_data
```

## Phase 2-2: 楽曲ローダー

```mermaid
classDiagram
    class song_loader {
        +load_all(string songs_dir) song_load_result
        +load_chart(string path) chart_parse_result
    }

    class song_data {
        +song_meta meta
        +vector~string~ chart_paths
        +string directory
    }

    class song_load_result {
        +vector~song_data~ songs
        +vector~string~ errors
    }

    song_loader --> song_load_result : returns
    song_loader --> chart_parse_result : returns
    song_load_result --> song_data : contains
    song_data --> song_meta
```

## Phase 3-1: 入力ハンドラー

```mermaid
classDiagram
    class input_handler {
        -key_config key_config_
        -array~bool~ prev_state_
        -array~bool~ curr_state_
        +update() void
        +is_lane_just_pressed(int lane) bool
        +is_lane_held(int lane) bool
        +is_lane_just_released(int lane) bool
    }

    class key_config {
        +array~int, 4~ keys_4
        +array~int, 6~ keys_6
        +get_lane_keys(int key_count) span~int~
    }

    input_handler --> key_config
```

## Phase 3-2: タイミングエンジン

```mermaid
classDiagram
    class timing_engine {
        -vector~timing_event~ timing_events_
        -int resolution_
        +init(vector~timing_event~ events, int resolution) void
        +tick_to_ms(int tick) double
        +ms_to_tick(double ms) int
        +get_bpm_at(int tick) float
    }

    timing_engine --> timing_event
```

## Phase 3-3: 判定システム

```mermaid
classDiagram
    class judge_system {
        -array~double, 5~ judge_windows_
        -vector~note_state~ note_states_
        +init(vector~note_data~ notes, timing_engine engine) void
        +update(double current_ms, input_handler input) void
        +get_last_judge() optional~judge_event~
        +get_note_states() vector~note_state~
    }

    class note_state {
        +note_data note_ref
        +double target_ms
        +bool judged
        +judge_result result
        +bool holding
    }

    class judge_event {
        +judge_result result
        +double offset_ms
        +int lane
    }

    judge_system --> note_state : manages
    judge_system --> judge_event : emits
    note_state --> judge_result
    judge_event --> judge_result
```

## Phase 3-4: スコア・ゲージ

```mermaid
classDiagram
    class score_system {
        -int score_
        -int combo_
        -int max_combo_
        -array~int, 5~ judge_counts_
        -int total_notes_
        -int fast_count_
        -int slow_count_
        -double offset_sum_
        +init(int total_notes) void
        +on_judge(judge_event event) void
        +get_score() int
        +get_combo() int
        +get_result_data() result_data
    }

    class gauge {
        -float value_
        +on_judge(judge_result result) void
        +get_value() float
        +is_cleared() bool
    }

    score_system --> judge_event
    score_system --> result_data : produces
    gauge --> judge_result
```
