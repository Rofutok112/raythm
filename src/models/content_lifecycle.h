#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace content_lifecycle {

inline std::string normalize_status(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) == 0;
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch == '-' || std::isspace(ch) != 0) {
            return '_';
        }
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string effective_review_status(const std::string& review_status,
                                           const std::string& lifecycle_status) {
    const std::string lifecycle = normalize_status(lifecycle_status);
    if (!lifecycle.empty() && lifecycle != "active") {
        return lifecycle;
    }
    const std::string review = normalize_status(review_status);
    if (!review.empty() && review != "active") {
        return review;
    }
    return lifecycle;
}

inline bool is_active(const std::string& review_status,
                      const std::string& lifecycle_status) {
    const std::string lifecycle = normalize_status(lifecycle_status);
    const std::string review = normalize_status(review_status);
    return (lifecycle.empty() || lifecycle == "active") &&
           (review.empty() || review == "active");
}

inline bool lifecycle_is_active(const std::string& lifecycle_status) {
    const std::string lifecycle = normalize_status(lifecycle_status);
    return lifecycle.empty() || lifecycle == "active";
}

inline bool is_pending_review(const std::string& review_status,
                              const std::string& lifecycle_status) {
    return effective_review_status(review_status, lifecycle_status) == "pending_review";
}

inline std::string display_label(const std::string& review_status,
                                 const std::string& lifecycle_status) {
    const std::string status = effective_review_status(review_status, lifecycle_status);
    if (status.empty() || status == "active") {
        return "";
    }
    if (status == "pending_review") {
        return "PENDING REVIEW";
    }
    if (status == "rejected") {
        return "REJECTED";
    }
    if (status == "archived") {
        return "ARCHIVED";
    }
    if (status == "draft") {
        return "DRAFT";
    }

    std::string label = status;
    std::replace(label.begin(), label.end(), '_', ' ');
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return label;
}

inline std::string sentence_label(const std::string& review_status,
                                  const std::string& lifecycle_status) {
    const std::string status = effective_review_status(review_status, lifecycle_status);
    if (status.empty() || status == "active") {
        return "";
    }
    if (status == "pending_review") {
        return "pending review";
    }
    std::string label = display_label(review_status, lifecycle_status);
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return label;
}

}  // namespace content_lifecycle
