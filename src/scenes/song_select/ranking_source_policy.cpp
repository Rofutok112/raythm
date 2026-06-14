#include "song_select/ranking_source_policy.h"

#include "network/auth_client.h"

namespace song_select::ranking_source_policy {
namespace {

bool has_queueable_link_for_server(const chart_option& chart, const std::string& server_url) {
    if (!can_use_online_chart_routes(chart)) {
        return false;
    }
    const std::string normalized_server_url = auth::normalize_server_url(server_url);
    if (normalized_server_url.empty()) {
        return false;
    }
    if (online_content::is_queueable(chart.online_identity) &&
        auth::normalize_server_url(chart.online_identity->server_url) == normalized_server_url) {
        return true;
    }
    for (const online_content::chart_identity& link : chart.remote_links) {
        if (online_content::is_queueable(link) &&
            auth::normalize_server_url(link.server_url) == normalized_server_url) {
            return true;
        }
    }
    return false;
}

}  // namespace

ranking_service::source personal_best_source_for_chart(const chart_option* chart) {
    if (chart == nullptr || !can_use_online_chart_routes(*chart)) {
        return ranking_service::source::local;
    }
    if (chart->source == content_source::official ||
        chart->source == content_source::community) {
        return ranking_service::source::online;
    }

    const auth::session_summary summary = auth::load_session_summary();
    return summary.logged_in && has_queueable_link_for_server(*chart, summary.server_url)
        ? ranking_service::source::online
        : ranking_service::source::local;
}

}  // namespace song_select::ranking_source_policy
