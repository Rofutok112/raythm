#include <cstdlib>
#include <iostream>
#include <string>

#include "mv/composition/mv_composition_event_authoring.h"
#include "mv/composition/mv_composition_event_evaluator.h"

namespace {

void expect(bool condition, const std::string& message, bool& ok) {
    if (!condition) {
        std::cerr << message << '\n';
        ok = false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    mv::composition::mv_composition composition =
        mv::composition::make_default_for_song("event-authoring-smoke", 30000.0);
    mv::composition::layer& layer = composition.layers.back();
    layer.id = "cue-layer";
    layer.source_data.type = "text";
    layer.source_data.text = "Verse";
    layer.visible = false;

    const auto flash = mv::composition::add_flash_cue(layer, composition, 1200.0, 24.0);
    expect(flash.changed && flash.created, "Expected flash cue to be created.", ok);
    expect(layer.effects.size() == 1 && layer.effects.front().type == "flash",
           "Expected flash cue to create a flash effect.",
           ok);
    expect(layer.event_triggers.size() == 1 &&
               layer.event_triggers.front().event == "mv.flash" &&
               layer.event_triggers.front().time_ms == 1200.0 &&
               layer.event_triggers.front().actions.size() == 1 &&
               layer.event_triggers.front().actions.front().effect_id == layer.effects.front().id,
           "Expected flash cue trigger shape.",
           ok);

    const auto flash_update = mv::composition::add_flash_cue(layer, composition, 1210.0, 24.0);
    expect(flash_update.changed && !flash_update.created,
           "Expected flash cue near the same time to update instead of duplicate.",
           ok);
    expect(layer.event_triggers.size() == 1 && layer.event_triggers.front().time_ms == 1210.0,
           "Expected flash cue update to preserve one trigger and update time.",
           ok);

    const auto show = mv::composition::add_show_cue(layer, 2400.0, 24.0);
    expect(show.changed && show.created, "Expected show cue to be created.", ok);
    const auto text = mv::composition::add_text_cue(layer, 3600.0, 24.0, "DROP");
    expect(text.changed && text.created, "Expected text cue to be created.", ok);
    expect(layer.source_data.text == "DROP", "Expected text cue authoring to update current text.", ok);
    expect(layer.event_triggers.size() == 3, "Expected three cues after flash/show/text authoring.", ok);
    expect(mv::composition::count_timeline_cues_near(layer, 3600.0, 24.0) == 1,
           "Expected one cue near lyric time.",
           ok);

    const auto before = mv::composition::apply_timeline_events(composition, 0.0, 1199.0);
    expect(before.matched_triggers == 0 && !layer.visible,
           "Expected cues not to fire before their time.",
           ok);
    const auto flash_applied = mv::composition::apply_timeline_events(composition, 1199.0, 1210.0);
    expect(flash_applied.matched_triggers == 1 && flash_applied.applied_actions == 1,
           "Expected flash cue to fire when crossed.",
           ok);
    const auto show_applied = mv::composition::apply_timeline_events(composition, 2399.0, 2400.0);
    expect(show_applied.matched_triggers == 1 && layer.visible,
           "Expected show cue to show the layer.",
           ok);
    layer.source_data.text = "Bridge";
    const auto text_applied = mv::composition::apply_timeline_events(composition, 3599.0, 3600.0);
    expect(text_applied.matched_triggers == 1 && layer.source_data.text == "DROP",
           "Expected text cue to update text when crossed.",
           ok);

    const int cleared = mv::composition::clear_timeline_cues_near(layer, 3600.0, 24.0);
    expect(cleared == 1 && mv::composition::count_timeline_cues_near(layer, 3600.0, 24.0) == 0,
           "Expected clearing cues near playhead to remove only matching cue.",
           ok);
    expect(layer.event_triggers.size() == 2, "Expected non-matching cues to remain after clear.", ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_composition_event_authoring smoke test passed\n";
    return EXIT_SUCCESS;
}
