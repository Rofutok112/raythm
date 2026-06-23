#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "mv/composition/mv_composition.h"
#include "mv/composition/mv_composition_edit_history.h"

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
        mv::composition::make_default_for_song("history-smoke", 10000.0);
    const std::string initial_selected = composition.objects.back().id;

    mv::composition::edit_history history;
    history.reset(composition, initial_selected);
    expect(history.size() == 1, "Expected reset to create one snapshot.", ok);
    expect(history.is_clean(composition), "Expected reset composition to be clean.", ok);
    expect(!history.can_undo(), "Expected no undo immediately after reset.", ok);

    composition.objects.back().name = "Renamed";
    history.commit(composition, initial_selected, "Rename Layer");
    expect(history.can_undo(), "Expected undo after commit.", ok);
    expect(!history.can_redo(), "Expected redo stack to be empty after commit.", ok);
    expect(!history.is_clean(composition), "Expected edited composition to be dirty.", ok);
    expect(history.undo_label() == "Rename Layer", "Expected undo label to describe current edit.", ok);

    mv::composition::edit_snapshot snapshot;
    expect(history.undo(snapshot), "Expected undo to succeed.", ok);
    expect(snapshot.composition.objects.back().name != "Renamed", "Expected undo to restore object name.", ok);
    expect(snapshot.selected_layer_id == initial_selected, "Expected selected layer to round-trip through undo.", ok);
    expect(history.can_redo(), "Expected redo after undo.", ok);
    expect(history.is_clean(snapshot.composition), "Expected undo to return to clean composition.", ok);

    expect(history.redo(snapshot), "Expected redo to succeed.", ok);
    expect(snapshot.composition.objects.back().name == "Renamed", "Expected redo to reapply object name.", ok);
    expect(!history.is_clean(snapshot.composition), "Expected redone composition to be dirty.", ok);

    history.mark_clean(snapshot.composition);
    expect(history.is_clean(snapshot.composition), "Expected mark_clean to update clean fingerprint.", ok);

    mv::composition::layer layer;
    layer.id = "new-layer";
    layer.name = "New Layer";
    composition = snapshot.composition;
    composition.objects.push_back(layer);
    history.commit(composition, layer.id, "Add Layer");
    expect(history.can_undo(), "Expected undo after branch commit.", ok);
    expect(!history.can_redo(), "Expected branch commit to clear redo.", ok);

    composition.objects.back().components.push_back(mv::composition::make_transform_component());
    mv::composition::component renderer = mv::composition::make_component("text");
    renderer.text = "Hello MV";
    renderer.fill = "#6ee7b7";
    composition.objects.back().components.push_back(renderer);
    history.commit(composition, layer.id, "Edit Source");
    expect(history.undo(snapshot), "Expected undo of source edit to succeed.", ok);
    expect(mv::composition::renderable_component(snapshot.composition.objects.back()) == nullptr,
           "Expected source text to undo.",
           ok);
    expect(history.redo(snapshot), "Expected redo of source edit to succeed.", ok);
    const mv::composition::component* redone_renderer =
        mv::composition::renderable_component(snapshot.composition.objects.back());
    expect(redone_renderer != nullptr &&
               redone_renderer->text == "Hello MV" &&
               redone_renderer->fill == "#6ee7b7",
           "Expected source text and fill to redo.",
           ok);

    composition = snapshot.composition;
    const std::string before_reorder_first_id = composition.objects.front().id;
    const std::string before_reorder_last_id = composition.objects.back().id;
    std::swap(composition.objects.front(), composition.objects.back());
    composition.objects.front().order = 10;
    composition.objects.back().order = 20;
    history.commit(composition, before_reorder_last_id, "Reorder Layer");
    expect(history.undo(snapshot), "Expected undo of reorder to succeed.", ok);
    expect(snapshot.composition.objects.front().id == before_reorder_first_id,
           "Expected reorder undo to restore first layer.",
           ok);
    expect(history.redo(snapshot), "Expected redo of reorder to succeed.", ok);
    expect(snapshot.composition.objects.front().id == before_reorder_last_id &&
               snapshot.selected_layer_id == before_reorder_last_id,
           "Expected reorder redo to restore order and selection.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }
    std::cout << "mv_composition_edit_history smoke test passed\n";
    return EXIT_SUCCESS;
}
