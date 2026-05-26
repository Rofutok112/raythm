#include "title/create_tools_view.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "network/server_environment.h"
#include "scene_common.h"
#include "title/local_content_index.h"
#include "theme.h"
#include "ui_draw.h"
#include "virtual_screen.h"

namespace {

constexpr float kCreateToolButtonHeight = 50.0f;
constexpr float kCreateToolButtonGap = 8.0f;
constexpr float kCreateToolColumnGap = 12.0f;
constexpr float kCreatePanelSectionGap = 22.0f;
constexpr float kCreatePanelSectionLabelHeight = 24.0f;
constexpr int kCreateToolColumnCount = 2;

enum class create_tool_action {
    create_song,
    edit_song,
    import_song,
    export_song,
    upload_song,
    create_chart,
    edit_chart,
    import_chart,
    export_chart,
    upload_chart,
    edit_mv,
    manage_library,
};

struct create_tool_entry {
    std::string title;
    std::string detail;
    create_tool_action action;
    bool enabled = true;
    bool primary = false;
};

struct create_tool_section {
    std::string title;
    std::vector<create_tool_entry> entries;
};

Rectangle create_tool_rect(Rectangle section_rect, int index, bool primary) {
    if (primary) {
        return {
            section_rect.x,
            section_rect.y,
            section_rect.width,
            kCreateToolButtonHeight,
        };
    }

    const int column = index % kCreateToolColumnCount;
    const int row = index / kCreateToolColumnCount;
    const float width =
        (section_rect.width - kCreateToolColumnGap * static_cast<float>(kCreateToolColumnCount - 1)) /
        static_cast<float>(kCreateToolColumnCount);
    return {
        section_rect.x + static_cast<float>(column) * (width + kCreateToolColumnGap),
        section_rect.y + static_cast<float>(row) * (kCreateToolButtonHeight + kCreateToolButtonGap),
        width,
        kCreateToolButtonHeight,
    };
}

float create_tools_height(const std::vector<create_tool_entry>& entries) {
    if (entries.empty()) {
        return 0.0f;
    }

    int rows = 0;
    int secondary_count = 0;
    for (const create_tool_entry& entry : entries) {
        if (entry.primary) {
            rows += 1;
        } else {
            ++secondary_count;
        }
    }
    rows += (secondary_count + kCreateToolColumnCount - 1) / kCreateToolColumnCount;
    return static_cast<float>(rows) * kCreateToolButtonHeight +
           static_cast<float>(std::max(0, rows - 1)) * kCreateToolButtonGap;
}

std::optional<local_content_index::online_song_binding> song_upload_binding(const song_select::song_entry* song,
                                                                            const std::string& server_url) {
    if (song == nullptr || song->song.meta.song_id.empty() || server_url.empty()) {
        return std::nullopt;
    }
    return local_content_index::find_song_by_local(server_url, song->song.meta.song_id);
}

std::optional<local_content_index::online_chart_binding> chart_upload_binding(const song_select::chart_option* chart,
                                                                              const std::string& server_url) {
    if (chart == nullptr || chart->meta.chart_id.empty() || server_url.empty()) {
        return std::nullopt;
    }
    return local_content_index::find_chart_by_local(server_url, chart->meta.chart_id);
}

bool is_owned_origin(std::optional<local_content_index::online_origin> origin) {
    return origin.has_value() && *origin == local_content_index::online_origin::owned_upload;
}

std::vector<create_tool_section> build_create_tool_sections(const song_select::song_entry* song,
                                                            const song_select::chart_option* chart,
                                                            const std::string& session_server_url,
                                                            bool online_status_checking) {
    const std::string server_url = server_environment::normalize_url(session_server_url);
    const auto song_binding = song_upload_binding(song, server_url);
    const auto chart_binding = chart_upload_binding(chart, server_url);
    const std::optional<local_content_index::online_origin> song_origin =
        song_binding.has_value() ? std::optional<local_content_index::online_origin>(song_binding->origin) : std::nullopt;
    const std::optional<local_content_index::online_origin> chart_origin =
        chart_binding.has_value() ? std::optional<local_content_index::online_origin>(chart_binding->origin) : std::nullopt;
    const bool song_selected = song != nullptr;
    const bool chart_selected = chart != nullptr;
    const bool owned_song = is_owned_origin(song_origin);
    const bool owned_chart = is_owned_origin(chart_origin);
    const bool song_can_create_chart = song_selected;
    const bool linked_remote_song = song_origin.has_value();
    const bool song_can_upload =
        !online_status_checking &&
        song_selected;
    const bool chart_can_upload =
        !online_status_checking &&
        chart_selected &&
        linked_remote_song;

    std::string song_publish_title = "UPLOAD SONG";
    std::string song_publish_detail = "Publish selected song";
    if (!song_selected) {
        song_publish_title = "SELECT SONG";
        song_publish_detail = "Song publish unavailable";
    } else if (online_status_checking) {
        song_publish_title = "CHECKING SONG";
        song_publish_detail = "Verifying online status";
    } else if (owned_song) {
        song_publish_title = "UPDATE SONG";
        song_publish_detail = "Replace your upload";
    } else if (linked_remote_song) {
        song_publish_title = "UPDATE SONG";
        song_publish_detail = "Check edit permission";
    } else if (song->source_status == content_status::official) {
        song_publish_title = "OFFICIAL SONG";
        song_publish_detail = "Song is linked online";
    } else if (song->source_status == content_status::community) {
        song_publish_title = "COMMUNITY SONG";
        song_publish_detail = "Check edit permission";
    }

    std::string chart_publish_title = "UPLOAD CHART";
    std::string chart_publish_detail = linked_remote_song ? "Publish to this song" : "Upload song first";
    if (!chart_selected) {
        chart_publish_title = "SELECT CHART";
        chart_publish_detail = "Chart publish unavailable";
    } else if (online_status_checking) {
        chart_publish_title = "CHECKING CHART";
        chart_publish_detail = "Verifying online status";
    } else if (owned_chart) {
        chart_publish_title = "UPDATE CHART";
        chart_publish_detail = "Replace your upload";
    } else if (chart_origin.has_value()) {
        chart_publish_title = "UPDATE CHART";
        chart_publish_detail = "Check edit permission";
    } else if (chart->source_status == content_status::official) {
        chart_publish_title = "OFFICIAL CHART";
        chart_publish_detail = "Check edit permission";
    } else if (chart->source_status == content_status::community) {
        chart_publish_title = "COMMUNITY CHART";
        chart_publish_detail = "Check edit permission";
    }

    return {
        {
            "Song",
            {
                {song_publish_title, song_publish_detail, create_tool_action::upload_song, song_can_upload, true},
                {"NEW SONG", "Create package", create_tool_action::create_song, true, false},
                {"EDIT SONG", "Metadata", create_tool_action::edit_song, song_selected, false},
                {"IMPORT SONG", ".rpack", create_tool_action::import_song, true, false},
                {"EXPORT SONG", ".rpack", create_tool_action::export_song, song_selected, false},
            },
        },
        {
            "Chart",
            {
                {chart_publish_title, chart_publish_detail, create_tool_action::upload_chart, chart_can_upload, true},
                {"NEW CHART", "Add to song", create_tool_action::create_chart, song_can_create_chart, false},
                {"EDIT CHART", "Open editor", create_tool_action::edit_chart, chart_selected, false},
                {"IMPORT CHART", ".rchart", create_tool_action::import_chart, song_selected, false},
                {"EXPORT CHART", ".rchart", create_tool_action::export_chart, chart_selected, false},
            },
        },
        {
            "More",
            {
                {"MV EDITOR", "Visuals", create_tool_action::edit_mv, song_selected, false},
                {"LIBRARY", "Classic tools", create_tool_action::manage_library, true, false},
            },
        },
    };
}

void apply_action(title_play_view::update_result& result, create_tool_action action) {
    switch (action) {
    case create_tool_action::create_song: result.create_song_requested = true; break;
    case create_tool_action::edit_song: result.edit_song_requested = true; break;
    case create_tool_action::import_song: result.import_song_requested = true; break;
    case create_tool_action::export_song: result.export_song_requested = true; break;
    case create_tool_action::upload_song: result.upload_song_requested = true; break;
    case create_tool_action::create_chart: result.create_chart_requested = true; break;
    case create_tool_action::edit_chart: result.edit_chart_requested = true; break;
    case create_tool_action::import_chart: result.import_chart_requested = true; break;
    case create_tool_action::export_chart: result.export_chart_requested = true; break;
    case create_tool_action::upload_chart: result.upload_chart_requested = true; break;
    case create_tool_action::edit_mv: result.edit_mv_requested = true; break;
    case create_tool_action::manage_library: result.manage_library_requested = true; break;
    }
}

}  // namespace

namespace title_create_tools_view {

title_play_view::update_result update(const song_select::state& state,
                                      const title_play_view::layout& current,
                                      bool left_pressed,
                                      Vector2 mouse) {
    title_play_view::update_result result;
    if (!left_pressed) {
        return result;
    }

    const song_select::song_entry* song = song_select::selected_song(state);
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);
    const std::vector<create_tool_section> sections =
        build_create_tool_sections(song, chart, state.auth.server_url, state.catalog_loading);
    float section_y = current.ranking_list_rect.y;
    for (const create_tool_section& section : sections) {
        const float tools_y = section_y + kCreatePanelSectionLabelHeight;
        const Rectangle tools_rect{
            current.ranking_list_rect.x,
            tools_y,
            current.ranking_list_rect.width,
            current.ranking_list_rect.height - (tools_y - current.ranking_list_rect.y),
        };
        int primary_index = 0;
        int secondary_index = 0;
        for (const create_tool_entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
            if (!entry.primary) {
                const float secondary_top =
                    tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                rect.y = secondary_top +
                    static_cast<float>(index / kCreateToolColumnCount) *
                        (kCreateToolButtonHeight + kCreateToolButtonGap);
            }
            if (!entry.enabled || !CheckCollisionPointRec(mouse, rect)) {
                continue;
            }
            apply_action(result, entry.action);
            return result;
        }
        section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
    }

    return result;
}

void draw(const song_select::state& state, const draw_config& config) {
    const auto& t = *g_theme;
    const auto filtered = song_select::filtered_charts_for_selected_song(state);
    const song_select::song_entry* song = song_select::selected_song(state);
    const song_select::chart_option* chart = song_select::selected_chart_for(state, filtered);

    ui::draw_text_in_rect("CREATE", 22, config.current.ranking_header_rect,
                          with_alpha(t.text, config.alpha), ui::text_align::left);
    const std::vector<create_tool_section> sections =
        build_create_tool_sections(song, chart, state.auth.server_url, state.catalog_loading);
    const Vector2 mouse = virtual_screen::get_virtual_mouse();
    float section_y = config.current.ranking_list_rect.y;
    for (const create_tool_section& section : sections) {
        ui::draw_text_in_rect(section.title.c_str(), 14,
                              {config.current.ranking_list_rect.x, section_y,
                               config.current.ranking_list_rect.width, kCreatePanelSectionLabelHeight},
                              with_alpha(t.text_secondary, config.alpha), ui::text_align::left);
        const float tools_y = section_y + kCreatePanelSectionLabelHeight;
        const Rectangle tools_rect{
            config.current.ranking_list_rect.x,
            tools_y,
            config.current.ranking_list_rect.width,
            config.current.ranking_list_rect.height - (tools_y - config.current.ranking_list_rect.y),
        };
        int primary_index = 0;
        int secondary_index = 0;
        for (const create_tool_entry& entry : section.entries) {
            const int index = entry.primary ? primary_index++ : secondary_index++;
            Rectangle rect = create_tool_rect(tools_rect, index, entry.primary);
            if (!entry.primary) {
                const float secondary_top =
                    tools_y + static_cast<float>(primary_index) * (kCreateToolButtonHeight + kCreateToolButtonGap);
                rect.y = secondary_top +
                    static_cast<float>(index / kCreateToolColumnCount) *
                        (kCreateToolButtonHeight + kCreateToolButtonGap);
            }

            const bool hovered = entry.enabled && CheckCollisionPointRec(mouse, rect);
            const unsigned char row_alpha = !entry.enabled
                ? static_cast<unsigned char>(config.normal_row_alpha / 2)
                : hovered ? config.hover_row_alpha : config.normal_row_alpha;
            const Color action_color = entry.primary && entry.enabled
                ? (entry.title.find("UPDATE") != std::string::npos ? t.accent : t.success)
                : config.button_base;
            const Color fill = entry.primary && entry.enabled
                ? with_alpha(lerp_color(config.button_base, action_color, hovered ? 0.30f : 0.20f), row_alpha)
                : with_alpha(config.button_base, row_alpha);
            const Color border = entry.primary && entry.enabled
                ? with_alpha(action_color, static_cast<unsigned char>(std::min(255, static_cast<int>(row_alpha) + 38)))
                : with_alpha(t.border, row_alpha);
            ui::draw_rect_f(rect, fill);
            ui::draw_rect_lines(rect, entry.primary ? 1.6f : 1.2f, border);
            ui::draw_text_in_rect(entry.title.c_str(), entry.primary ? 16 : 13,
                                  {rect.x + 12.0f, rect.y + 5.0f, rect.width - 24.0f, 20.0f},
                                  with_alpha(entry.enabled ? t.text : t.text_muted, config.alpha),
                                  ui::text_align::left);
            ui::draw_text_in_rect(entry.detail.c_str(), 10,
                                  {rect.x + 12.0f, rect.y + 27.0f, rect.width - 24.0f, 15.0f},
                                  with_alpha(t.text_muted, config.alpha), ui::text_align::left);
        }
        section_y = tools_y + create_tools_height(section.entries) + kCreatePanelSectionGap;
    }
}

}  // namespace title_create_tools_view
