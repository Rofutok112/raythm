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
    const std::string initial_selected = composition.layers.back().id;

    mv::composition::edit_history history;
    history.reset(composition, initial_selected);
    expect(history.size() == 1, "Expected reset to create one snapshot.", ok);
    expect(history.is_clean(composition), "Expected reset composition to be clean.", ok);
    expect(!history.can_undo(), "Expected no undo immediately after reset.", ok);

    composition.layers.back().name = "Renamed";
    history.commit(composition, initial_selected, "Rename Layer");
    expect(history.can_undo(), "Expected undo after commit.", ok);
    expect(!history.can_redo(), "Expected redo stack to be empty after commit.", ok);
    expect(!history.is_clean(composition), "Expected edited composition to be dirty.", ok);
    expect(history.undo_label() == "Rename Layer", "Expected undo label to describe current edit.", ok);

    mv::composition::edit_snapshot snapshot;
    expect(history.undo(snapshot), "Expected undo to succeed.", ok);
    expect(snapshot.composition.layers.back().name != "Renamed", "Expected undo to restore layer name.", ok);
    expect(snapshot.selected_layer_id == initial_selected, "Expected selected layer to round-trip through undo.", ok);
    expect(history.can_redo(), "Expected redo after undo.", ok);
    expect(history.is_clean(snapshot.composition), "Expected undo to return to clean composition.", ok);

    expect(history.redo(snapshot), "Expected redo to succeed.", ok);
    expect(snapshot.composition.layers.back().name == "Renamed", "Expected redo to reapply layer name.", ok);
    expect(!history.is_clean(snapshot.composition), "Expected redone composition to be dirty.", ok);

    history.mark_clean(snapshot.composition);
    expect(history.is_clean(snapshot.composition), "Expected mark_clean to update clean fingerprint.", ok);

    mv::composition::layer layer;
    layer.id = "new-layer";
    layer.name = "New Layer";
    composition = snapshot.composition;
    composition.layers.push_back(layer);
    history.commit(composition, layer.id, "Add Layer");
    expect(history.can_undo(), "Expected undo after branch commit.", ok);
    expect(!history.can_redo(), "Expected branch commit to clear redo.", ok);

    composition.layers.back().source_data.type = "text";
    composition.layers.back().source_data.text = "Hello MV";
    composition.layers.back().source_data.fill = "#6ee7b7";
    history.commit(composition, layer.id, "Edit Source");
    expect(history.undo(snapshot), "Expected undo of source edit to succeed.", ok);
    expect(snapshot.composition.layers.back().source_data.text.empty(),
           "Expected source text to undo.",
           ok);
    expect(history.redo(snapshot), "Expected redo of source edit to succeed.", ok);
    expect(snapshot.composition.layers.back().source_data.text == "Hello MV" &&
               snapshot.composition.layers.back().source_data.fill == "#6ee7b7",
           "Expected source text and fill to redo.",
           ok);

    composition = snapshot.composition;
    const std::string before_reorder_first_id = composition.layers.front().id;
    const std::string before_reorder_last_id = composition.layers.back().id;
    std::swap(composition.layers.front(), composition.layers.back());
    composition.layers.front().z = 10;
    composition.layers.back().z = 20;
    history.commit(composition, before_reorder_last_id, "Reorder Layer");
    expect(history.undo(snapshot), "Expected undo of reorder to succeed.", ok);
    expect(snapshot.composition.layers.front().id == before_reorder_first_id,
           "Expected reorder undo to restore first layer.",
           ok);
    expect(history.redo(snapshot), "Expected redo of reorder to succeed.", ok);
    expect(snapshot.composition.layers.front().id == before_reorder_last_id &&
               snapshot.selected_layer_id == before_reorder_last_id,
           "Expected reorder redo to restore order and selection.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }
    std::cout << "mv_composition_edit_history smoke test passed\n";
    return EXIT_SUCCESS;
}
