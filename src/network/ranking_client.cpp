#include "network/ranking_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "network/http_client.h"
#include "network/json_helpers.h"

namespace {
namespace json = network::json;
using http_response = network::http::response;
using network::http::send_request;

std::optional<ranking_service::entry> parse_ranking_entry(const std::string& content) {
    std::string player_display_name;
    if (const auto player_object = json::extract_object(content, "player"); player_object.has_value()) {
        player_display_name = json::extract_string(*player_object, "display_name").value_or("");
    }

    const auto placement = json::extract_int(content, "placement");
    const auto accuracy = json::extract_float(content, "accuracy");
    const auto is_full_combo = json::extract_bool(content, "is_full_combo");
    const auto max_combo = json::extract_int(content, "max_combo");
    const auto score = json::extract_int(content, "score");
    const auto recorded_at = json::extract_string(content, "recorded_at");
    const auto verified = json::extract_bool(content, "verified");
    const std::string clear_rank_label = json::extract_string(content, "clear_rank").value_or("");
    if (!placement.has_value() ||
        !accuracy.has_value() ||
        !is_full_combo.has_value() ||
        !max_combo.has_value() ||
        !score.has_value() ||
        !recorded_at.has_value() ||
        !verified.has_value()) {
        return std::nullopt;
    }

    return ranking_service::entry{
        .placement = *placement,
        .player_display_name = player_display_name,
        .accuracy = *accuracy,
        .is_full_combo = *is_full_combo,
        .max_combo = *max_combo,
        .score = *score,
        .recorded_at = *recorded_at,
        .verified = *verified,
        .resolved_clear_rank = clear_rank_label.empty()
            ? scoring_ruleset_runtime::compute_rank_for(
                scoring_ruleset_runtime::current_ruleset(),
                *accuracy,
                *is_full_combo)
            : [&clear_rank_label]() {
                if (clear_rank_label == "ss") return rank::ss;
                if (clear_rank_label == "s") return rank::s;
                if (clear_rank_label == "aa") return rank::aa;
                if (clear_rank_label == "a") return rank::a;
                if (clear_rank_label == "b") return rank::b;
                if (clear_rank_label == "c") return rank::c;
                return rank::f;
            }(),
    };
}

std::optional<ranking_client::listing_response> parse_listing_response(const std::string& body) {
    const bool available = json::extract_bool(body, "available").value_or(false);
    const std::string message = json::extract_string(body, "message").value_or("");

    ranking_client::listing_response listing;
    listing.available = available;
    listing.message = message;

    const std::optional<std::string> entries_array = json::extract_array(body, "entries");
    if (!entries_array.has_value()) {
        return listing;
    }

    for (const std::string& object : json::extract_objects_from_array(*entries_array)) {
        const auto entry = parse_ranking_entry(object);
        if (entry.has_value()) {
            listing.entries.push_back(*entry);
        }
    }

    return listing;
}

std::string build_ranking_url(const std::string& server_url, const std::string& chart_id, int limit) {
    const int clamped_limit = std::max(1, limit);
    return server_url + "/charts/" + chart_id + "/rankings?page=1&pageSize=" + std::to_string(clamped_limit);
}

std::string build_submit_ranking_url(const std::string& server_url, const std::string& chart_id) {
    return server_url + "/charts/" + chart_id + "/rankings";
}

std::string build_chart_manifest_url(const std::string& server_url, const std::string& chart_id) {
    return server_url + "/charts/" + chart_id + "/manifest";
}

std::string build_song_manifest_url(const std::string& server_url, const std::string& song_id) {
    return server_url + "/songs/" + song_id + "/manifest";
}

std::string build_official_manifest_url(const std::string& server_url, const std::string& chart_id) {
    return server_url + "/charts/" + chart_id + "/official-manifest";
}

std::string judge_result_label(judge_result result) {
    switch (result) {
        case judge_result::perfect: return "perfect";
        case judge_result::great: return "great";
        case judge_result::good: return "good";
        case judge_result::bad: return "bad";
        case judge_result::miss: return "miss";
    }

    return "miss";
}

std::string build_note_results_json(const std::vector<note_result_entry>& note_results) {
    std::string json = "[";
    for (size_t i = 0; i < note_results.size(); ++i) {
        const note_result_entry& entry = note_results[i];
        if (i > 0) {
            json += ",";
        }
        json += "{";
        json += "\"event_index\":" + std::to_string(entry.event_index) + ",";
        json += "\"result\":\"" + judge_result_label(entry.result) + "\",";
        json += "\"offset_ms\":" + std::to_string(entry.offset_ms);
        json += "}";
    }
    json += "]";
    return json;
}

std::string build_submit_payload(const result_data& result,
                                 const std::string& recorded_at,
                                 const std::string& ruleset_version) {
    return "{"
        "\"recorded_at\":\"" + json::escape_string(recorded_at) + "\","
        "\"ruleset_version\":\"" + json::escape_string(ruleset_version) + "\","
        "\"note_results\":" + build_note_results_json(result.note_results) +
        "}";
}

std::optional<ranking_client::submit_response> parse_submit_response(const std::string& body) {
    ranking_client::submit_response response;
    response.available = json::extract_bool(body, "available").value_or(true);
    response.updated = json::extract_bool(body, "updated").value_or(false);
    response.message = json::extract_string(body, "message").value_or("");

    if (const auto entry_object = json::extract_object(body, "entry"); entry_object.has_value()) {
        response.entry = parse_ranking_entry(*entry_object);
    }

    return response;
}

std::optional<ranking_client::chart_manifest> parse_chart_manifest_response(const std::string& body) {
    const auto available = json::extract_bool(body, "available");
    const auto chart_id = json::extract_string(body, "chartId")
        .value_or(json::extract_string(body, "chart_id").value_or(""));
    const auto song_id = json::extract_string(body, "songId")
        .value_or(json::extract_string(body, "song_id").value_or(""));
    if (!available.has_value() || chart_id.empty() || song_id.empty()) {
        return std::nullopt;
    }

    return ranking_client::chart_manifest{
        .available = *available,
        .message = json::extract_string(body, "message").value_or(""),
        .content_source = json::extract_string(body, "content_source").value_or(
            json::extract_string(body, "contentSource").value_or("")),
        .chart_id = chart_id,
        .song_id = song_id,
        .song_json_sha256 = json::extract_string(body, "songJsonSha256").value_or(
            json::extract_string(body, "song_json_sha256").value_or("")),
        .song_json_fingerprint = json::extract_string(body, "song_json_fingerprint").value_or(
            json::extract_string(body, "songJsonFingerprint").value_or("")),
        .audio_sha256 = json::extract_string(body, "audioSha256").value_or(
            json::extract_string(body, "audio_sha256").value_or("")),
        .jacket_sha256 = json::extract_string(body, "jacketSha256").value_or(
            json::extract_string(body, "jacket_sha256").value_or("")),
        .chart_sha256 = json::extract_string(body, "chartSha256").value_or(
            json::extract_string(body, "chart_sha256").value_or("")),
        .chart_fingerprint = json::extract_string(body, "chart_fingerprint").value_or(
            json::extract_string(body, "chartFingerprint").value_or("")),
    };
}

std::optional<ranking_client::song_manifest> parse_song_manifest_response(const std::string& body) {
    const auto available = json::extract_bool(body, "available");
    const auto song_id = json::extract_string(body, "songId")
        .value_or(json::extract_string(body, "song_id").value_or(""));
    if (!available.has_value() || song_id.empty()) {
        return std::nullopt;
    }

    return ranking_client::song_manifest{
        .available = *available,
        .message = json::extract_string(body, "message").value_or(""),
        .content_source = json::extract_string(body, "content_source").value_or(
            json::extract_string(body, "contentSource").value_or("")),
        .song_id = song_id,
        .song_json_sha256 = json::extract_string(body, "songJsonSha256").value_or(
            json::extract_string(body, "song_json_sha256").value_or("")),
        .song_json_fingerprint = json::extract_string(body, "song_json_fingerprint").value_or(
            json::extract_string(body, "songJsonFingerprint").value_or("")),
        .audio_sha256 = json::extract_string(body, "audioSha256").value_or(
            json::extract_string(body, "audio_sha256").value_or("")),
        .jacket_sha256 = json::extract_string(body, "jacketSha256").value_or(
            json::extract_string(body, "jacket_sha256").value_or("")),
    };
}

std::optional<ranking_client::scoring_ruleset> parse_scoring_ruleset_response(const std::string& body) {
    const auto active = json::extract_bool(body, "active");
    const auto accepted_input = json::extract_string(body, "accepted_input");
    const auto ruleset_version = json::extract_string(body, "ruleset_version");
    const auto score_model = json::extract_string(body, "score_model");
    const auto max_score = json::extract_int(body, "max_score");
    const auto judges_object = json::extract_object(body, "judges");
    const auto thresholds_array = json::extract_array(body, "rank_thresholds");
    if (!active.has_value() || !accepted_input.has_value() || !ruleset_version.has_value() ||
        !score_model.has_value() || !max_score.has_value() ||
        !judges_object.has_value() || !thresholds_array.has_value()) {
        return std::nullopt;
    }

    const auto perfect = json::extract_int(*judges_object, "perfect");
    const auto great = json::extract_int(*judges_object, "great");
    const auto good = json::extract_int(*judges_object, "good");
    const auto bad = json::extract_int(*judges_object, "bad");
    const auto miss = json::extract_int(*judges_object, "miss");
    if (!perfect.has_value() || !great.has_value() || !good.has_value() ||
        !bad.has_value() || !miss.has_value()) {
        return std::nullopt;
    }

    std::vector<scoring_ruleset_runtime::rank_threshold> rank_thresholds;
    for (const std::string& object : json::extract_objects_from_array(*thresholds_array)) {
        const auto rank_label = json::extract_string(object, "rank");
        const auto min_accuracy = json::extract_float(object, "min_accuracy");
        const auto requires_full_combo = json::extract_bool(object, "requires_full_combo");
        if (!rank_label.has_value() || !min_accuracy.has_value() || !requires_full_combo.has_value()) {
            return std::nullopt;
        }

        rank parsed_rank = rank::f;
        if (*rank_label == "ss") {
            parsed_rank = rank::ss;
        } else if (*rank_label == "s") {
            parsed_rank = rank::s;
        } else if (*rank_label == "aa") {
            parsed_rank = rank::aa;
        } else if (*rank_label == "a") {
            parsed_rank = rank::a;
        } else if (*rank_label == "b") {
            parsed_rank = rank::b;
        } else if (*rank_label == "c") {
            parsed_rank = rank::c;
        }

        rank_thresholds.push_back(scoring_ruleset_runtime::rank_threshold{
            .clear_rank = parsed_rank,
            .min_accuracy = *min_accuracy,
            .requires_full_combo = *requires_full_combo,
        });
    }

    return ranking_client::scoring_ruleset{
        .active = *active,
        .accepted_input = *accepted_input,
        .ruleset_version = *ruleset_version,
        .score_model = *score_model,
        .max_score = *max_score,
        .judge_values = {*perfect, *great, *good, *bad, *miss},
        .rank_thresholds = std::move(rank_thresholds),
    };
}

}  // namespace

namespace ranking_client {

operation_result fetch_chart_ranking(const std::string& server_url,
                                     const std::string& access_token,
                                     const std::string& chart_id,
                                     int limit) {
    if (server_url.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "No server URL is configured.",
            .listing = std::nullopt,
        };
    }

    if (access_token.empty()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to view online rankings.",
            .listing = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        build_ranking_url(server_url, chart_id, limit),
        {
            {"Accept", "application/json"},
            {"Authorization", "Bearer " + access_token},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = response.error_message,
            .listing = std::nullopt,
        };
    }

    if (response.status_code == 401) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to view online rankings.",
            .listing = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .unauthorized = false,
            .message = json::extract_string(response.body, "message").value_or("Failed to load online rankings."),
            .listing = std::nullopt,
        };
    }

    const std::optional<listing_response> listing = parse_listing_response(response.body);
    if (!listing.has_value()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "Server returned an unexpected ranking response.",
            .listing = std::nullopt,
        };
    }

    return {
        .success = true,
        .unauthorized = false,
        .message = listing->message,
        .listing = listing,
    };
}

submit_operation_result submit_chart_ranking(const std::string& server_url,
                                             const std::string& access_token,
                                             const std::string& chart_id,
                                             const result_data& result,
                                             const std::string& recorded_at,
                                             const std::string& ruleset_version) {
    if (server_url.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "No server URL is configured.",
            .submission = std::nullopt,
        };
    }

    if (access_token.empty()) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to submit online rankings.",
            .submission = std::nullopt,
        };
    }

    const http_response response = send_request(
        "POST",
        build_submit_ranking_url(server_url, chart_id),
        {
            {"Accept", "application/json"},
            {"Authorization", "Bearer " + access_token},
            {"Content-Type", "application/json"},
            {"User-Agent", "raythm/0.1"},
        },
        build_submit_payload(result, recorded_at, ruleset_version));

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = response.error_message,
            .submission = std::nullopt,
        };
    }

    if (response.status_code == 401) {
        return {
            .success = false,
            .unauthorized = true,
            .message = "Sign in to submit online rankings.",
            .submission = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .unauthorized = false,
            .message = json::extract_string(response.body, "message").value_or("Failed to submit online ranking."),
            .submission = std::nullopt,
        };
    }

    const std::optional<submit_response> submission = parse_submit_response(response.body);
    if (!submission.has_value()) {
        return {
            .success = false,
            .unauthorized = false,
            .message = "Server returned an unexpected ranking response.",
            .submission = std::nullopt,
        };
    }

    return {
        .success = true,
        .unauthorized = false,
        .message = submission->message,
        .submission = submission,
    };
}

scoring_ruleset_operation_result fetch_scoring_ruleset(const std::string& server_url) {
    if (server_url.empty()) {
        return {
            .success = false,
            .message = "No server URL is configured.",
            .ruleset = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        server_url + "/scoring/ruleset",
        {
            {"Accept", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .ruleset = std::nullopt,
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = json::extract_string(response.body, "message").value_or("Failed to fetch scoring ruleset."),
            .ruleset = std::nullopt,
        };
    }

    const std::optional<scoring_ruleset> ruleset = parse_scoring_ruleset_response(response.body);
    if (!ruleset.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected scoring ruleset response.",
            .ruleset = std::nullopt,
        };
    }

    return {
        .success = true,
        .message = {},
        .ruleset = ruleset,
    };
}

manifest_operation_result fetch_chart_manifest(const std::string& server_url,
                                               const std::string& chart_id) {
    if (server_url.empty()) {
        return {
            .success = false,
            .message = "No server URL is configured.",
            .manifest = std::nullopt,
        };
    }

    http_response response = send_request(
        "GET",
        build_chart_manifest_url(server_url, chart_id),
        {
            {"Accept", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .manifest = std::nullopt,
        };
    }

    if (response.status_code == 404) {
        response = send_request(
            "GET",
            build_official_manifest_url(server_url, chart_id),
            {
                {"Accept", "application/json"},
                {"User-Agent", "raythm/0.1"},
            });
        if (!response.error_message.empty()) {
            return {
                .success = false,
                .message = response.error_message,
                .manifest = std::nullopt,
            };
        }
        if (response.status_code == 404) {
            return {
                .success = true,
                .message = "Manifest not found.",
                .manifest = chart_manifest{
                    .available = false,
                    .message = "Manifest not found.",
                    .chart_id = chart_id,
                },
            };
        }
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = json::extract_string(response.body, "message").value_or("Failed to fetch chart manifest."),
            .manifest = std::nullopt,
        };
    }

    const std::optional<chart_manifest> manifest = parse_chart_manifest_response(response.body);
    if (!manifest.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected chart manifest response.",
            .manifest = std::nullopt,
        };
    }

    return {
        .success = true,
        .message = manifest->message,
        .manifest = manifest,
    };
}

song_manifest_operation_result fetch_song_manifest(const std::string& server_url,
                                                   const std::string& song_id) {
    if (server_url.empty()) {
        return {
            .success = false,
            .message = "No server URL is configured.",
            .manifest = std::nullopt,
        };
    }

    const http_response response = send_request(
        "GET",
        build_song_manifest_url(server_url, song_id),
        {
            {"Accept", "application/json"},
            {"User-Agent", "raythm/0.1"},
        });

    if (!response.error_message.empty()) {
        return {
            .success = false,
            .message = response.error_message,
            .manifest = std::nullopt,
        };
    }

    if (response.status_code == 404) {
        return {
            .success = true,
            .message = "Song manifest not found.",
            .manifest = song_manifest{
                .available = false,
                .message = "Song manifest not found.",
                .song_id = song_id,
            },
        };
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return {
            .success = false,
            .message = json::extract_string(response.body, "message").value_or("Failed to fetch song manifest."),
            .manifest = std::nullopt,
        };
    }

    const std::optional<song_manifest> manifest = parse_song_manifest_response(response.body);
    if (!manifest.has_value()) {
        return {
            .success = false,
            .message = "Server returned an unexpected song manifest response.",
            .manifest = std::nullopt,
        };
    }

    return {
        .success = true,
        .message = manifest->message,
        .manifest = manifest,
    };
}

}  // namespace ranking_client
