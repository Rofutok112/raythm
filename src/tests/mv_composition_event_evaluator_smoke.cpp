#include <cstdlib>
#include <iostream>
#include <string>

#include "mv/composition/mv_composition_event_evaluator.h"
#include "mv/composition/mv_composition_evaluator.h"

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
        mv::composition::make_default_for_song("event-smoke", 30000.0);
    mv::composition::layer& layer = composition.layers.back();
    layer.id = "event-layer";
    layer.source_data.type = "text";
    layer.source_data.text = "Verse";
    layer.transform_data.opacity = 0.2f;
    layer.visible = false;
    layer.effects.push_back({
        .id = "fx-flash",
        .type = "flash",
        .target = "transform.opacity",
        .amount = 0.75f,
    });

    mv::composition::event_trigger chorus;
    chorus.event = "section.chorus";
    chorus.actions.push_back({
        .type = "showLayer",
    });
    chorus.actions.push_back({
        .type = "setProperty",
        .property = "source.text",
        .value = "CHORUS",
    });
    chorus.actions.push_back({
        .type = "setProperty",
        .property = "transform.opacity",
        .value = "0.4",
    });
    chorus.actions.push_back({
        .type = "triggerEffect",
        .effect_id = "fx-flash",
    });
    layer.event_triggers.push_back(chorus);

    const auto ignored = mv::composition::apply_event(composition, "section.drop", 1200.0);
    expect(ignored.matched_triggers == 0 && ignored.applied_actions == 0,
           "Expected unrelated events to be ignored.",
           ok);

    const auto applied = mv::composition::apply_event(composition, "section.chorus", 1200.0);
    expect(applied.matched_triggers == 1 && applied.applied_actions == 4,
           "Expected matching event actions to apply.",
           ok);
    expect(layer.visible, "Expected showLayer action to show layer.", ok);
    expect(layer.source_data.text == "CHORUS", "Expected setProperty to update text.", ok);
    expect(layer.transform_data.opacity == 0.4f, "Expected setProperty to update opacity.", ok);
    expect(layer.start_ms == 1200.0, "Expected triggerEffect to restart layer local time.", ok);
    const mv::composition::transform flashed = mv::composition::evaluate_transform(layer, 1200.0);
    expect(flashed.opacity > 0.8f, "Expected triggered flash effect to evaluate.", ok);

    layer.visible = false;
    layer.source_data.text = "Bridge";
    mv::composition::event_trigger timed;
    timed.event = "mv.flash";
    timed.time_ms = 2400.0;
    timed.actions.push_back({
        .type = "showLayer",
    });
    timed.actions.push_back({
        .type = "setText",
        .value = "DROP",
    });
    layer.event_triggers.push_back(timed);

    const auto before_timed = mv::composition::apply_timeline_events(composition, 1200.0, 2399.0);
    expect(before_timed.matched_triggers == 0 && before_timed.applied_actions == 0,
           "Expected timeline trigger to wait until timeMs is crossed.",
           ok);
    expect(!layer.visible && layer.source_data.text == "Bridge",
           "Expected timeline trigger to leave layer unchanged before crossing.",
           ok);

    const auto timed_applied = mv::composition::apply_timeline_events(composition, 2399.0, 2400.0);
    expect(timed_applied.matched_triggers == 1 && timed_applied.applied_actions == 2,
           "Expected timeline trigger to apply when crossing timeMs.",
           ok);
    expect(layer.visible && layer.source_data.text == "DROP",
           "Expected timeline trigger actions to update the layer.",
           ok);

    layer.visible = false;
    const auto backwards = mv::composition::apply_timeline_events(composition, 2500.0, 1000.0);
    expect(backwards.matched_triggers == 0 && backwards.applied_actions == 0 && !layer.visible,
           "Expected backwards timeline movement to avoid replaying triggers.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "mv_composition_event_evaluator smoke test passed\n";
    return EXIT_SUCCESS;
}
