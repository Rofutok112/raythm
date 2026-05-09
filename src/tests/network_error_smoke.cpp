#include <cstdlib>
#include <iostream>
#include <string>

#include "network/network_error.h"

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

    const network::error_classification maintenance = network::classify_http_error(
        503,
        "{\"error\":\"maintenance\",\"message\":\"Database migration in progress.\"}",
        "Service unavailable.",
        "120");
    expect(maintenance.is_maintenance(), "503 maintenance response was not classified as maintenance.", ok);
    expect(maintenance.retry_after == "120", "Retry-After header was not preserved.", ok);
    expect(maintenance.message == "Maintenance mode: Database migration in progress. Retry after 120.",
           "Maintenance message was not formatted as expected.", ok);

    const network::error_classification maintenance_without_message = network::classify_http_error(
        503,
        "{\"error\":\"maintenance\"}",
        "Service unavailable.",
        "");
    expect(maintenance_without_message.is_maintenance(), "Maintenance without message was not classified.", ok);
    expect(maintenance_without_message.message == "Maintenance mode: raythm-Server is temporarily unavailable.",
           "Default maintenance message was not formatted as expected.", ok);

    const network::error_classification regular_error = network::classify_http_error(
        503,
        "{\"error\":\"overloaded\",\"message\":\"Try again later.\"}",
        "Service unavailable.",
        "60");
    expect(!regular_error.is_maintenance(), "Non-maintenance 503 was misclassified.", ok);
    expect(regular_error.message == "Try again later.", "Regular error message was not preserved.", ok);
    expect(regular_error.retry_after == "60", "Regular Retry-After header was not exposed.", ok);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
