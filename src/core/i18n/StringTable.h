/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/data/Json.h"

namespace fossynd::core::i18n
{

// A dependency-free lookup table of localised UI strings (ADR-0016).
//
// Keys are stable identifiers (`hud.objective.eliminate`) and values are the
// display strings for one language. The table is engine-free and I/O-free: the
// caller reads the JSON (a file or the copy embedded at build time) and hands it
// in, so the same logic is unit-tested with a plain compiler and stays SDL-free
// (ADR-0002).
//
// It deliberately holds no global state: a `StringTable` is a value an owner
// creates and passes to whatever draws text, so two tables (e.g. two languages)
// can coexist. Missing keys never throw; they fall back (see `get`).
class StringTable
{
public:
    // Loads a flat JSON object of `"key": "value"` string pairs. Returns false
    // and sets `error` if `root` is not an object or a value is not a string.
    // On failure the table is left unchanged.
    bool loadFromJson(const Json& root, std::string& error);

    // Loads a string table from a JSON file. Returns false and sets `error` on a
    // missing/unreadable file or a schema error.
    bool loadFromFile(const std::string& path, std::string& error);

    // Resolves and loads the table for `locale`, so a translator can add a
    // language without touching C++.
    //
    // The locale file is looked up with `defaultLangPaths(exeDir, locale)` (the
    // first existing path wins, e.g. `assets/lang/de.json`). The table is first
    // filled from `fallback` (the English table the game passes), then the
    // locale's own keys are overlaid on top, so a present key wins and any key
    // the locale omits keeps its English value — an untranslated string shows
    // English rather than the raw key.
    //
    // `locale` "en" (or empty) is a no-op: the table simply becomes `fallback`.
    // Returns false and sets `error` when no locale file was found or it was
    // malformed; the table is still populated from `fallback` and remains usable,
    // so a missing translation is a warning, never a crash.
    bool loadForLocale(const std::string& locale, const std::string& exeDir,
                       const StringTable& fallback, std::string& error);

    // Returns the string for `key`. When the key is missing, returns `fallback`
    // if it is non-null, otherwise the key itself — so a missing key is always
    // visible in the UI rather than silently blank.
    std::string get(const std::string& key, const char* fallback = nullptr) const;

    bool has(const std::string& key) const;

    // Number of loaded entries. Zero means "empty table", which callers treat as
    // "no localisation": `get` then returns the fallback/key.
    std::size_t size() const { return entries_.size(); }

private:
    std::unordered_map<std::string, std::string> entries_;
};

// The built-in fallback locations for a language file, in priority order, so an
// installed game finds `en.json` without a config file. `exeDir` may be empty.
// Relative entries are resolved against the process working directory.
std::vector<std::string> defaultLangPaths(const std::string& exeDir, const std::string& lang);

} // namespace fossynd::core::i18n
