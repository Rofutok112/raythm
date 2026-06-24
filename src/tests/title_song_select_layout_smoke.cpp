#include "title/seamless_song_select_layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

bool close(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.001f;
}

bool same_rect(Rectangle lhs, Rectangle rhs) {
    return close(lhs.x, rhs.x) &&
           close(lhs.y, rhs.y) &&
           close(lhs.width, rhs.width) &&
           close(lhs.height, rhs.height);
}

bool contains(Rectangle outer, Rectangle inner) {
    return inner.x >= outer.x &&
           inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

}  // namespace

int main() {
    bool ok = true;

    const Rectangle origin = {840.0f, 564.0f, 240.0f, 90.0f};
    const title_play_view::layout play =
        title_play_view::make_mode_layout(1.0f, origin, title_play_view::mode::play);
    const title_play_view::layout create =
        title_play_view::make_mode_layout(1.0f, origin, title_play_view::mode::create);

    expect(same_rect(play.song_column, create.song_column),
           "Expected Create to reuse the Play song column layout.", ok);
    expect(same_rect(play.main_column, create.main_column),
           "Expected Create to reuse the Play main column layout.", ok);
    expect(same_rect(play.ranking_column, create.ranking_column),
           "Expected Create to reuse the Play right column layout.", ok);

    const Rectangle top = title_play_view::right_top_pane_rect(play.ranking_column);
    const Rectangle bottom = title_play_view::right_bottom_pane_rect(play.ranking_column);
    expect(close(top.y, play.ranking_column.y) && close(top.height, 306.0f),
           "Expected right top pane to preserve the selected-chart summary area.", ok);
    expect(close(bottom.y, play.ranking_column.y + 337.0f) &&
               close(bottom.height, play.ranking_column.height - 337.0f),
           "Expected right bottom pane to preserve the ranking/tools area.", ok);
    expect(contains(bottom, title_play_view::mod_button_rect(play.ranking_column)),
           "Expected MODS button to stay inside the right bottom pane.", ok);
    expect(contains(bottom, title_play_view::start_button_rect(play.ranking_column)),
           "Expected START button to stay inside the right bottom pane.", ok);

    const Rectangle song_list = title_play_view::song_list_rect(play);
    expect(contains(play.song_column, song_list),
           "Expected embedded song list to stay inside the song column.", ok);
    expect(contains(play.main_column, title_play_view::center_jacket_rect(play)),
           "Expected center jacket to stay inside the main column.", ok);
    expect(contains(play.main_column, title_play_view::center_detail_rect(play)),
           "Expected center details to stay inside the main column.", ok);

    const float level = 12.3f;
    const float round_tripped = title_play_view::level_from_filter_t(title_play_view::level_filter_t(level));
    expect(close(round_tripped, level),
           "Expected level filter mapping to round-trip useful levels.", ok);
    expect(close(title_play_view::level_from_filter_t(1.0f), title_play_view::kChartFilterMaxLevel),
           "Expected the right edge of the level filter to mean max level.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "title_song_select_layout smoke test passed\n";
    return EXIT_SUCCESS;
}
