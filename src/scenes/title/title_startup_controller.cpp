#include "title/title_startup_controller.h"

#include <algorithm>
#include <vector>

#include "localization/localization.h"
#include "shared/loading_screen_view.h"
#include "theme.h"
#include "tween.h"
#include "ui/ui_font.h"
#include "ui_draw.h"

namespace {

constexpr float kStartupProgressMin = 0.08f;
constexpr float kStartupProgressCatalog = 0.68f;
constexpr float kStartupProgressFonts = 0.78f;
constexpr float kStartupProgressScoring = 0.88f;

void append_literal_with_translation(std::vector<std::string>& texts, const char* literal) {
    texts.emplace_back(literal);
    const char* translated = localization::tr_literal(literal);
    if (translated != nullptr) {
        texts.emplace_back(translated);
    }
}

void append_common_localized_texts(std::vector<std::string>& texts) {
    for (int i = 0; i < localization::text_key_count(); ++i) {
        const auto key = static_cast<localization::text_key>(i);
        texts.emplace_back(localization::tr(key, localization::locale::english));
        texts.emplace_back(localization::tr(key, localization::locale::japanese));
    }

    constexpr const char* kLiterals[] = {
        "PLAY", "MULTIPLAY", "BROWSE", "CREATE", "SELECT",
        "Solo song select.", "Room battles soon.", "Browse and download.",
        "Create, import, export.", "This route is still warming up.",
        "Rooms", "Create Room", "Sign in from the account menu before joining multiplayer.",
        "No rooms yet.", "No chart selected", "host", "lobby", "playing", "online", "away",
        "Players", "READY", "WAIT", "Beatmap queue", "Add song", "No queued songs yet.",
        "Installed", "Not installed", "Up", "Down", "Remove", "Chat", "Message...", "Send", "Playing:",
        "Queue: host only", "Queue: all players", "Leave", "Cancel Ready",
        "Ready (queue empty)", "Ready", "Ready (download needed)", "Download", "Start",
        "Room name", "Optional", "Players:", "Max", "Room password", "Joining...", "Join",
        "Starting...", "Loaded. Waiting for players...", "Waiting for other players...", "Starting in",
        "No playable song package found", "Failed to load selected chart",
        "No chart found for selected key mode", "ESC: Back to Song Select",
        "HOME", "CREATE TOOLS", "ACCOUNT", "SETTINGS",
        "Manage account", "Verified profile", "Email verification pending",
        "SONG", "CHARTS", "Overview", "Rising", "Hidden gems", "Recommended",
        "Needs charts", "Source", "Official", "Community", "Mine", "Search",
        "songs / artists / tags", "Find charts to download", "Downloaded",
        "Not downloaded", "Open Song", "Open Local", "Remove Local",
        "Download Chart", "Open Chart", "Loading local catalog...",
        "Preparing UI text...", "Preparing scoring cache...", "Ready.",
        "Catalog loaded with warnings."
    };
    for (const char* literal : kLiterals) {
        append_literal_with_translation(texts, literal);
    }
}

std::vector<std::string> startup_font_preload_texts(const song_select::state& state) {
    std::vector<std::string> texts = {
        "Home", "Overview", "Rising", "Hidden gems", "Recommended", "Needs charts",
        "Source", "Official", "Community", "Mine", "Search", "songs / artists / tags",
        "SONG", "CHARTS", "Find charts to download", "All", "Downloaded",
        "Not downloaded", "Level", "Keys", "BPM", "Open Song", "Open Local",
        "Remove Local", "Download Chart", "Open Chart", "Loading local catalog...",
        "Preparing scoring cache...", "Ready.", "Catalog loaded with warnings.",
        "No songs found", "No songs found yet.", "JACKET", "Settings", "Profile",
        "Recent Activity", "Rankings", "Max Combo", "Accuracy",
        "あいうえおかきくけこさしすせそたちつてとなにぬねの",
        "はひふへほまみむめもやゆよらりるれろわをん",
        "アイウエオカキクケコサシスセソタチツテトナニヌネノ",
        "ハヒフヘホマミムメモヤユヨラリルレロワヲン",
        "がぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽ",
        "ガギグゲゴザジズゼゾダヂヅデドバビブベボパピプペポ",
        "ー・、。！？「」（）[]/ feat. Lv. charts plays notes by"
    };
    append_common_localized_texts(texts);

    for (const song_select::song_entry& song : state.songs) {
        texts.push_back(song.song.meta.title);
        texts.push_back(song.song.meta.artist);
        texts.push_back(song.song.meta.genre);
        for (const std::string& genre : song.song.meta.genres) {
            texts.push_back(genre);
        }
        for (const std::string& keyword : song.song.meta.keywords) {
            texts.push_back(keyword);
        }
        for (const song_select::chart_option& chart : song.charts) {
            texts.push_back(chart.meta.difficulty);
            texts.push_back(chart.meta.chart_author);
        }
    }
    return texts;
}

}  // namespace

namespace title_startup_controller {

void reset(state& startup) {
    startup.loading = true;
    startup.catalog_requested = false;
    startup.fonts_preload_started = false;
    startup.fonts_preloaded = false;
    startup.scoring_requested = false;
    startup.load_complete = false;
    startup.load_failed = false;
    startup.progress_visual = kStartupProgressMin;
    startup.catalog_progress = 0.0f;
    startup.loading_message = "Initializing audio...";
}

void update(state& startup, const update_context& context) {
    if (!startup.loading) {
        return;
    }

    if (!startup.catalog_requested) {
        startup.catalog_requested = true;
        startup.loading_message = "Loading local catalog...";
        context.request_play_catalog_reload(
            context.preferred_song_id,
            context.preferred_chart_id,
            title_catalog::policy_for(title_catalog::reload_mode::fast_startup,
                                      context.sync_media_on_catalog_apply));
        return;
    }

    if (context.play_catalog_loading()) {
        if (context.catalog_progress) {
            const load_progress catalog_progress = context.catalog_progress();
            startup.catalog_progress = catalog_progress.progress;
            startup.loading_message = catalog_progress.message.empty()
                ? "Loading local catalog..."
                : catalog_progress.message;
        } else {
            startup.loading_message = "Loading local catalog...";
        }
        return;
    }

    if (!context.play_state.catalog_loaded_once) {
        return;
    }

    startup.catalog_progress = 1.0f;

    if (!startup.fonts_preload_started) {
        startup.fonts_preload_started = true;
        startup.loading_message = "Preparing UI text...";
        return;
    }

    if (!startup.fonts_preloaded) {
        startup.loading_message = "Preparing UI text...";
        ui::preload_text_glyphs(startup_font_preload_texts(context.play_state));
        startup.fonts_preloaded = true;
        return;
    }

    if (!startup.scoring_requested) {
        startup.scoring_requested = true;
        startup.load_failed = !context.play_state.load_errors.empty();
        context.reload_online_catalog();
        if (context.play_state.auth.logged_in) {
            context.restore_auth();
        }
        context.request_scoring_ruleset_warm(false);
        startup.loading = false;
        startup.load_complete = true;
        startup.loading_message = startup.load_failed ? "Catalog loaded with warnings." : "Ready.";
        if (startup.load_failed) {
            context.home_status_message = context.play_state.load_errors.empty()
                ? "Catalog loaded with warnings."
                : context.play_state.load_errors.front();
        }
        return;
    }
}

void draw_loading(state& startup, float dt) {
    float base_progress = kStartupProgressMin;
    if (startup.catalog_requested) {
        base_progress = kStartupProgressMin +
                        (kStartupProgressCatalog - kStartupProgressMin) *
                            std::clamp(startup.catalog_progress, 0.0f, 1.0f);
    }
    if (startup.fonts_preload_started) {
        base_progress = kStartupProgressFonts;
    }
    if (startup.scoring_requested) {
        base_progress = kStartupProgressScoring;
    }
    if (startup.load_complete) {
        base_progress = 1.0f;
    }
    startup.progress_visual = tween::damp(startup.progress_visual, base_progress, dt, 5.0f, 0.0005f);
    loading_screen_view::draw({
        .message = startup.loading_message.c_str(),
        .progress = startup.progress_visual,
        .error = startup.load_failed,
    });
}

void draw_status(const state& startup) {
    const Rectangle status_rect = {520.0f, 704.0f, 880.0f, 34.0f};
    ui::draw_text_in_rect(startup.loading_message.c_str(), 18, status_rect,
                          startup.load_failed ? g_theme->error : g_theme->text_muted);
}

}  // namespace title_startup_controller
