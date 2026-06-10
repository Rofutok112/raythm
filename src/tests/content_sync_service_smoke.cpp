#include "services/content_sync_service.h"

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

}  // namespace

int main() {
    bool ok = true;

    expect(content_sync_service::source_from_status(content_status::official) == content_source::official,
           "Expected official legacy status to map to official source.",
           ok);
    expect(content_sync_service::source_from_status(content_status::modified) == content_source::unknown,
           "Expected modified legacy status not to imply a content source.",
           ok);
    expect(content_sync_service::sync_from_status(content_status::modified) == content_sync_state::modified,
           "Expected modified legacy status to map to modified sync state.",
           ok);

    expect(content_sync_service::legacy_status_for_display({
               .source = content_source::official,
               .sync = content_sync_state::clean,
           }) == content_status::official,
           "Expected clean official content to display as official.",
           ok);
    expect(content_sync_service::legacy_status_for_display({
               .source = content_source::official,
               .sync = content_sync_state::modified,
           }) == content_status::modified,
           "Expected modified state to display independently from official source.",
           ok);
    expect(content_sync_service::legacy_status_for_display({
               .source = content_source::community,
               .sync = content_sync_state::update_available,
           }) == content_status::update,
           "Expected update state to display independently from community source.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "content_sync_service smoke test passed\n";
    return EXIT_SUCCESS;
}
