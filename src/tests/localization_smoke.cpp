#include <cstdlib>
#include <iostream>
#include <string>

#include "localization/localization.h"

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

    expect(localization::parse_locale_code("en") == localization::locale::english,
           "Expected en to parse as English.",
           ok);
    expect(localization::parse_locale_code("ja") == localization::locale::japanese,
           "Expected ja to parse as Japanese.",
           ok);
    expect(!localization::parse_locale_code("xx").has_value(),
           "Expected unknown locale code to be rejected.",
           ok);
    expect(std::string(localization::locale_code(localization::locale::english)) == "en",
           "Expected English to serialize as en.",
           ok);
    expect(std::string(localization::locale_code(localization::locale::japanese)) == "ja",
           "Expected Japanese to serialize as ja.",
           ok);

    for (int i = 0; i < localization::text_key_count(); ++i) {
        const auto key = static_cast<localization::text_key>(i);
        expect(localization::has_translation(key, localization::locale::english),
               "Expected every localization key to have English text.",
               ok);
    }

    localization::set_current_locale(localization::locale::japanese);
    expect(std::string(localization::tr(localization::text_key::settings)) == "設定",
           "Expected settings key to return Japanese text.",
           ok);
    expect(std::string(localization::tr_literal("SETTINGS")) == "設定",
           "Expected literal lookup to translate existing UI labels.",
           ok);
    expect(std::string(localization::tr_literal("Profile Links")) == "プロフィールリンク",
           "Expected profile settings labels to translate.",
           ok);
    expect(std::string(localization::tr_literal("SAVE LINKS")) == "リンクを保存",
           "Expected profile link actions to translate.",
           ok);
    expect(std::string(localization::tr_literal("CHANGE IMAGE")) == "画像を変更",
           "Expected profile image actions to translate.",
           ok);
    expect(std::string(localization::tr_literal("Saving profile image...")) == "プロフィール画像を保存中...",
           "Expected profile image status text to translate.",
           ok);
    localization::set_current_locale(localization::locale::english);
    expect(std::string(localization::tr_literal("SETTINGS")) == "SETTINGS",
           "Expected literal lookup to preserve English labels in English locale.",
           ok);

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "localization smoke test passed\n";
    return EXIT_SUCCESS;
}
