/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/data/Json.h"
#include "core/i18n/MissionText.h"
#include "core/i18n/StringTable.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <set>
#include <string>
#include <vector>

using namespace fossynd::core;
using fossynd::core::i18n::StringTable;
using fossynd::core::i18n::defaultLangPaths;

namespace
{

// The `{placeholder}` tokens in a format string, e.g. `{count}`. Two strings
// with the same token set can be substituted by the same `format` call, so a
// translation cannot silently drop a value.
std::set<std::string> placeholdersIn(const std::string& text)
{
    std::set<std::string> tokens;
    std::size_t i = 0;
    while ((i = text.find('{', i)) != std::string::npos)
    {
        const std::size_t end = text.find('}', i + 1);
        if (end == std::string::npos)
        {
            break;
        }
        tokens.insert(text.substr(i, end - i + 1));
        i = end + 1;
    }
    return tokens;
}

// The embedded 5x7 bitmap font covers ASCII 32..126 only (src/game/Font.cpp),
// so any other byte renders as a blank glyph.
bool isRenderableAscii(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return c >= 32 && c <= 126;
    });
}

// The `"key": "value"` pairs in a string-table JSON object, ignoring the
// `_comment` documentation key.
std::vector<std::pair<std::string, std::string>> tableEntries(const Json& root, std::string& error)
{
    std::vector<std::pair<std::string, std::string>> entries;
    if (!root.isObject())
    {
        error = "expected a JSON object";
        return entries;
    }
    for (const auto& entry : root.asObject())
    {
        if (!entry.first.empty() && entry.first[0] == '_')
        {
            continue;
        }
        entries.emplace_back(entry.first, entry.second.asString());
    }
    return entries;
}

// A comma-separated key list for a readable parity-failure message.
std::string joinKeys(const std::vector<std::string>& keys)
{
    std::string out;
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        if (i != 0)
        {
            out += ", ";
        }
        out += keys[i];
    }
    return out;
}

// Every shipped locale file (assets/lang/*.json except the English reference),
// sorted so a failure is deterministic.
std::vector<std::filesystem::path> shippedLocaleFiles()
{
    const std::filesystem::path dir = std::string(FOSSYND_SOURCE_DIR) + "/assets/lang";
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.path().extension() == ".json" && entry.path().filename() != "en.json")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

const std::string& englishPath()
{
    static const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/lang/en.json";
    return path;
}

// Every shipped mission id, taken from the file stem of
// assets/data/missions/*.json. The id is what `missionNameKey` derives the
// localisation key from, so the directory — not a hardcoded list — is the
// authoritative mission set: a new mission file cannot be forgotten here.
std::vector<std::string> shippedMissionIds()
{
    const std::filesystem::path dir =
        std::string(FOSSYND_SOURCE_DIR) + "/assets/data/missions";
    std::vector<std::string> ids;
    std::error_code ec;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (entry.path().extension() == ".json")
        {
            ids.push_back(entry.path().stem().string());
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

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

FOSSYND_TEST(I18n, LocaleOverlayWinsAndDoesNotLeakIntoFallback)
{
    const std::string srcDir = FOSSYND_SOURCE_DIR;

    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(srcDir + "/assets/lang/en.json", error));

    // German overrides the keys it defines; the overlay must not leak back into
    // the fallback table. (Missing-key retention is covered per-key by
    // LocaleOverlayWinsOverFallbackPerKey, which cannot drift with de.json.)
    StringTable german;
    FOSSYND_CHECK(german.loadForLocale("de", srcDir, english, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK_EQ(german.get("hud.squad"), std::string("TRUPPE"));
    FOSSYND_CHECK_EQ(english.get("hud.squad"), std::string("SQUAD"));
}

FOSSYND_TEST(I18n, ShippedLocalesMatchTheEnglishKeySet)
{
    // Every shipped locale must carry exactly the English key set, so no
    // translation silently ships an untranslated (English-fallback) string and
    // none invents a key the UI never reads. This is the #125 parity gate.
    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(englishPath(), error));

    Json root;
    std::string parseError;
    FOSSYND_CHECK(Json::parseFile(englishPath(), root, parseError));
    FOSSYND_CHECK(parseError.empty());

    std::string entryError;
    const auto englishEntries = tableEntries(root, entryError);
    FOSSYND_CHECK(entryError.empty());
    FOSSYND_CHECK(!englishEntries.empty());

    std::set<std::string> englishKeys;
    for (const auto& entry : englishEntries)
    {
        englishKeys.insert(entry.first);
    }

    const auto locales = shippedLocaleFiles();
    FOSSYND_CHECK(!locales.empty());
    for (const std::filesystem::path& localePath : locales)
    {
        Json localeRoot;
        std::string parseError;
        FOSSYND_CHECK(Json::parseFile(localePath.string(), localeRoot, parseError));
        FOSSYND_CHECK(parseError.empty());

        std::string localeEntryError;
        const auto localeEntries = tableEntries(localeRoot, localeEntryError);
        FOSSYND_CHECK(localeEntryError.empty());

        std::set<std::string> localeKeys;
        for (const auto& entry : localeEntries)
        {
            localeKeys.insert(entry.first);
        }

        // Compare the actual key sets, not just the counts: a locale that drops
        // one English key and invents another has the right size but is broken,
        // and would silently show a raw key or an English fallback.
        std::vector<std::string> missing;
        std::set_difference(englishKeys.begin(), englishKeys.end(), localeKeys.begin(),
                            localeKeys.end(), std::back_inserter(missing));
        std::vector<std::string> extra;
        std::set_difference(localeKeys.begin(), localeKeys.end(), englishKeys.begin(),
                            englishKeys.end(), std::back_inserter(extra));

        if (!missing.empty() || !extra.empty())
        {
            std::string message = localePath.filename().string() + ": key set mismatch";
            if (!missing.empty())
            {
                message += " | missing: " + joinKeys(missing);
            }
            if (!extra.empty())
            {
                message += " | extra: " + joinKeys(extra);
            }
            ::fossynd::test::reportFailure(__FILE__, __LINE__, message);
        }
    }
}

FOSSYND_TEST(I18n, ShippedLocalesKeepPlaceholdersAndAreRenderable)
{
    // Two failure modes this guards against:
    //   1. a translation that drops or renames a {placeholder}, which would make
    //      the format call print the raw token;
    //   2. a translation that uses a character the embedded ASCII font cannot
    //      draw (e.g. an umlaut), which would render as a blank glyph.
    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(englishPath(), error));

    Json englishRoot;
    std::string parseError;
    FOSSYND_CHECK(Json::parseFile(englishPath(), englishRoot, parseError));
    std::string entryError;
    const auto englishEntries = tableEntries(englishRoot, entryError);

    for (const std::filesystem::path& localePath : shippedLocaleFiles())
    {
        Json localeRoot;
        std::string localeParseError;
        FOSSYND_CHECK(Json::parseFile(localePath.string(), localeRoot, localeParseError));

        std::string localeEntryError;
        const auto localeEntries = tableEntries(localeRoot, localeEntryError);
        for (const auto& localeEntry : localeEntries)
        {
            const std::string& key = localeEntry.first;
            const std::string& value = localeEntry.second;
            FOSSYND_CHECK(isRenderableAscii(value));

            const auto it = std::find_if(englishEntries.begin(), englishEntries.end(),
                                         [&](const auto& e) { return e.first == key; });
            FOSSYND_CHECK(it != englishEntries.end());
            if (it != englishEntries.end())
            {
                FOSSYND_CHECK(placeholdersIn(value) == placeholdersIn(it->second));
            }
        }
    }
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

FOSSYND_TEST(I18n, MissionTextKeyDerivation)
{
    using fossynd::core::i18n::missionDescKey;
    using fossynd::core::i18n::missionIdFromPath;
    using fossynd::core::i18n::missionNameKey;

    FOSSYND_CHECK_EQ(missionNameKey("demo"), std::string("mission.demo.name"));
    FOSSYND_CHECK_EQ(missionDescKey("demo"), std::string("mission.demo.desc"));

    // An id that is not a safe key segment yields no key (so localisation is
    // skipped) rather than an ambiguous dotted key.
    FOSSYND_CHECK_EQ(missionNameKey(""), std::string(""));
    FOSSYND_CHECK_EQ(missionNameKey("a.b"), std::string(""));
    FOSSYND_CHECK_EQ(missionNameKey("Demo"), std::string(""));
    FOSSYND_CHECK_EQ(missionDescKey("has space"), std::string(""));

    // A mission id is the file stem, so a content path maps to a stable id.
    FOSSYND_CHECK_EQ(missionIdFromPath("assets/data/missions/demo.json"), std::string("demo"));
    FOSSYND_CHECK_EQ(missionIdFromPath("/usr/share/fossynd/missions/escort.json"),
                     std::string("escort"));
    FOSSYND_CHECK_EQ(missionIdFromPath("demo"), std::string("demo"));
    FOSSYND_CHECK_EQ(missionIdFromPath(""), std::string(""));
}

FOSSYND_TEST(I18n, MissionTextFallsBackToAuthoredTextAndIsIdempotent)
{
    using fossynd::core::i18n::localizeMissionText;
    using fossynd::core::i18n::missionNameKey;

    std::string error;
    const Json root = Json::parse(R"({ "mission.demo.name": "Downtown Sweep" })", error);
    StringTable table;
    FOSSYND_CHECK(table.loadFromJson(root, error));

    // A present key resolves to the table value.
    FOSSYND_CHECK_EQ(localizeMissionText(table, missionNameKey("demo"), "Raw Authored Name"),
                     std::string("Downtown Sweep"));
    // A missing key leaves the mission's own text untouched.
    FOSSYND_CHECK_EQ(localizeMissionText(table, missionNameKey("absent"), "Raw Authored Name"),
                     std::string("Raw Authored Name"));
    // No key at all (unsafe id) also leaves the authored text.
    FOSSYND_CHECK_EQ(localizeMissionText(table, "", "Raw Authored Name"),
                     std::string("Raw Authored Name"));

    // Applying the resolver to its own result is safe: the second pass still
    // resolves from the table, so the value is stable (idempotent).
    const std::string once = localizeMissionText(table, missionNameKey("demo"), "Raw Authored Name");
    FOSSYND_CHECK_EQ(localizeMissionText(table, missionNameKey("demo"), once), once);

    // An empty table value is treated as "not translated", never blanking copy.
    const Json blank = Json::parse(R"({ "mission.demo.name": "" })", error);
    StringTable blankTable;
    FOSSYND_CHECK(blankTable.loadFromJson(blank, error));
    FOSSYND_CHECK_EQ(localizeMissionText(blankTable, missionNameKey("demo"), "Raw Authored Name"),
                     std::string("Raw Authored Name"));
}

FOSSYND_TEST(I18n, ShippedMissionsHaveLocalisedTextInEveryLocale)
{
    using fossynd::core::i18n::localizeMissionText;
    using fossynd::core::i18n::missionDescKey;
    using fossynd::core::i18n::missionNameKey;

    // The shipped missions are the files in assets/data/missions; the id is the
    // file stem. Deriving the set here (rather than hardcoding it) means a new
    // mission without translations fails this test instead of slipping through.
    const std::vector<std::string> missionIds = shippedMissionIds();
    FOSSYND_CHECK(!missionIds.empty());

    const std::string srcDir = FOSSYND_SOURCE_DIR;
    StringTable english;
    std::string error;
    FOSSYND_CHECK(english.loadFromFile(srcDir + "/assets/lang/en.json", error));

    for (const std::string& id : missionIds)
    {
        // English is the reference: the authored copy is present verbatim.
        FOSSYND_CHECK(english.has(missionNameKey(id)));
        FOSSYND_CHECK(english.has(missionDescKey(id)));

        // Every shipped locale must localise both fields; a missing key would
        // silently show English to a non-English player.
        for (const std::filesystem::path& localePath : shippedLocaleFiles())
        {
            StringTable locale;
            FOSSYND_CHECK(locale.loadForLocale(localePath.stem().string(), srcDir, english, error));
            FOSSYND_CHECK(locale.has(missionNameKey(id)));
            FOSSYND_CHECK(locale.has(missionDescKey(id)));
            // Resolution yields a non-empty, renderable string in that locale.
            const std::string name =
                localizeMissionText(locale, missionNameKey(id), std::string());
            FOSSYND_CHECK(!name.empty());
            FOSSYND_CHECK(isRenderableAscii(name));
        }
    }
}
