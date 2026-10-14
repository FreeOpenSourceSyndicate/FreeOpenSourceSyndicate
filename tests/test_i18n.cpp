/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Json.h"
#include "core/i18n/StringTable.h"

using namespace fossynd::core;
using fossynd::core::i18n::StringTable;
using fossynd::core::i18n::defaultLangPaths;

FOSSYND_TEST(I18n, LoadsKeyValuePairs)
{
    std::string error;
    const Json root = Json::parse(R"({
        "hud.squad": "SQUAD",
        "hud.objective.eliminate": "OBJECTIVE: ELIMINATE ALL {count} ENEMY AGENTS"
    })",
                                  error);
    FOSSYND_CHECK(error.empty());

    StringTable table;
    FOSSYND_CHECK(table.loadFromJson(root, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(table.size(), static_cast<std::size_t>(2));
    FOSSYND_CHECK(table.has("hud.squad"));
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("SQUAD"));
    FOSSYND_CHECK_EQ(table.get("hud.objective.eliminate"),
                     std::string("OBJECTIVE: ELIMINATE ALL {count} ENEMY AGENTS"));
}

FOSSYND_TEST(I18n, MissingKeyFallsBackToLiteralThenKey)
{
    std::string error;
    const Json root = Json::parse(R"({ "present": "here" })", error);
    StringTable table;
    FOSSYND_CHECK(table.loadFromJson(root, error));

    // A caller-supplied literal wins when the key is missing.
    FOSSYND_CHECK_EQ(table.get("absent", "FALLBACK"), std::string("FALLBACK"));
    // With no literal, the key itself is returned (never blank, never throws).
    FOSSYND_CHECK_EQ(table.get("absent"), std::string("absent"));
    FOSSYND_CHECK(!table.has("absent"));
    // A present key ignores the fallback.
    FOSSYND_CHECK_EQ(table.get("present", "FALLBACK"), std::string("here"));
}

FOSSYND_TEST(I18n, RejectsNonObjectAndNonStringValues)
{
    std::string error;

    // Root must be an object, not an array.
    const Json arrayRoot = Json::parse(R"([ "a", "b" ])", error);
    StringTable table;
    FOSSYND_CHECK(!table.loadFromJson(arrayRoot, error));
    FOSSYND_CHECK(!error.empty());

    // A non-string value is a schema error.
    const Json badValue = Json::parse(R"({ "hp": 42 })", error);
    FOSSYND_CHECK(!table.loadFromJson(badValue, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(I18n, FailedLoadLeavesTableIntact)
{
    std::string error;
    const Json good = Json::parse(R"({ "hud.squad": "SQUAD" })", error);
    StringTable table;
    FOSSYND_CHECK(table.loadFromJson(good, error));

    // A malformed second load must not clear the good entries.
    const Json bad = Json::parse(R"({ "oops": 1 })", error);
    FOSSYND_CHECK(!table.loadFromJson(bad, error));
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("SQUAD"));
}

FOSSYND_TEST(I18n, LoadsShippedEnglishTable)
{
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/lang/en.json";
    StringTable table;
    std::string error;
    FOSSYND_CHECK(table.loadFromFile(path, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(table.size() > 0);
    // Spot-check the keys the HUD reads (issue #35 acceptance).
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("SQUAD"));
    FOSSYND_CHECK(table.has("hud.objective.eliminate"));
    FOSSYND_CHECK(table.has("title.footer"));
}

FOSSYND_TEST(I18n, MissingFileIsAnError)
{
    StringTable table;
    std::string error;
    FOSSYND_CHECK(!table.loadFromFile("assets/lang/does-not-exist.json", error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(I18n, DefaultLangPathsAreRepoRelativeFirst)
{
    const auto paths = defaultLangPaths("", "en");
    FOSSYND_CHECK(!paths.empty());
    FOSSYND_CHECK_EQ(paths.front(), std::string("assets/lang/en.json"));
}

FOSSYND_TEST(I18n, LocaleOverlaysFallbackAndKeepsMissingKeysEnglish)
{
    const std::string srcDir = FOSSYND_SOURCE_DIR;

    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(srcDir + "/assets/lang/en.json", error));

    // The German stub is partial: a key it defines wins, a key it omits keeps
    // its English value (never the raw key).
    StringTable german;
    FOSSYND_CHECK(german.loadForLocale("de", srcDir, english, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(german.get("hud.squad"), std::string("TRUPPE"));
    FOSSYND_CHECK_EQ(german.get("title.footer"), english.get("title.footer"));
    FOSSYND_CHECK(german.get("title.footer") != std::string("title.footer"));
    // The overlay must not leak into the fallback table.
    FOSSYND_CHECK_EQ(english.get("hud.squad"), std::string("SQUAD"));
}

FOSSYND_TEST(I18n, LocaleEnglishIsTheFallbackUnchanged)
{
    const std::string srcDir = FOSSYND_SOURCE_DIR;

    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(srcDir + "/assets/lang/en.json", error));

    StringTable table;
    FOSSYND_CHECK(table.loadForLocale("en", srcDir, english, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(table.size(), english.size());
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("SQUAD"));
}

FOSSYND_TEST(I18n, UnknownLocaleFallsBackToEnglishAndReportsError)
{
    const std::string srcDir = FOSSYND_SOURCE_DIR;

    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(srcDir + "/assets/lang/en.json", error));

    // No locale file: the load reports the miss but the table still holds the
    // English strings, so a missing translation never blanks the UI.
    StringTable table;
    FOSSYND_CHECK(!table.loadForLocale("xx", srcDir, english, error));
    FOSSYND_CHECK(!error.empty());
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("SQUAD"));
    FOSSYND_CHECK_EQ(table.size(), english.size());
}

FOSSYND_TEST(I18n, LocaleOverlayWinsOverFallbackPerKey)
{
    const std::string srcDir = FOSSYND_SOURCE_DIR;

    // A synthetic fallback with keys the German stub does not define, so the
    // per-key merge is asserted directly and cannot drift with the stub.
    std::string error;
    const Json root = Json::parse(R"({ "a": "EN-A", "b": "EN-B" })", error);
    StringTable fallback;
    FOSSYND_CHECK(fallback.loadFromJson(root, error));

    StringTable table;
    FOSSYND_CHECK(table.loadForLocale("de", srcDir, fallback, error));
    // "hud.squad" comes from de.json, not the synthetic fallback.
    FOSSYND_CHECK_EQ(table.get("hud.squad"), std::string("TRUPPE"));
    // Keys the locale omits survive from the fallback.
    FOSSYND_CHECK_EQ(table.get("a"), std::string("EN-A"));
    FOSSYND_CHECK_EQ(table.get("b"), std::string("EN-B"));
}
