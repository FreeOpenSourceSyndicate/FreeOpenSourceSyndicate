#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
"""String-table coverage guard (issues #191, #220).

The UI reads its strings from the table in ``assets/lang/en.json`` via
``tr(key, fallback)``. If the engine asks for a key the table does not carry,
the literal fallback is shown — so a localised run silently shows English (the
exact "stays English in a `de` run" failure #191 is about). This test makes that
impossible to merge.

Rules
-----
1. Every **key-shaped string literal** in ``src/`` must resolve in ``en.json``
   (#191).
2. Every shipped locale (``assets/lang/*.json`` other than ``en.json``) must
   carry **exactly** the English key set — no key missing, none invented (#220).
3. Every ``tr(key, fallback)`` call's fallback must equal ``en.json[key]``, so a
   table edit cannot leave a stale fallback behind (#238).
4. ``en.json`` itself must carry no duplicate key — it is the reference for rule
   2, so its own duplicates were the one place that check did not reach (#246).
5. Every shipped mission (a file in ``assets/data/missions/``) must have
   ``mission.<id>.name`` and ``mission.<id>.desc`` in ``en.json`` and every
   locale; these keys are derived at runtime from the file stem, so rule 1
   cannot see them (#246).
6. No value in ``en.json`` or any locale may be empty — ``StringTable::get``
   returns a present key verbatim, so an empty value wins over the ``tr()``
   fallback and blanks the UI element (#254).
7. Every value in ``en.json`` — not just the locales — must be renderable by the
   embedded font (ASCII 32..126). ``BitmapFont::draw`` maps any byte outside that
   range to a blank glyph, and English values are drawn directly (the window
   title, and ``mission.<id>.name`` / ``mission.<id>.desc``, which have no
   ``tr()`` fallback), so a non-ASCII English string ships an invisible glyph.
   The C++ renderability gate (``ShippedLocalesKeepPlaceholdersAndAreRenderable``)
   covers only the locale files, so the reference table was unchecked (#288).
8. Every ``substitute(<key lookup>, {{"name", ...}})`` call site must supply
   exactly the ``{tokens}`` its format string in ``en.json`` uses. ``substitute``
   leaves an unmatched ``{token}`` verbatim (``src/game/HudStyle.h``), so a call
   site that has drifted from its format string draws ``{count}`` / ``{x}`` on
   the HUD. Rule 3 cannot see this (the fallback is kept equal to ``en.json``),
   the C++ gate compares only a locale's placeholder set, and
   ``Game::objectiveSelfTest`` covers the objective banner alone (#332).

A key-shaped literal is a double-quoted string of the form
``segment(.segment)+`` where each segment is ``[a-z][A-Za-z0-9_]*`` (the shape
``tr()`` keys use, e.g. ``hud.objective.eliminate``). A literal whose final
segment is a known file extension (``foo.json``, ``bar.bmp``) is a path, not a
key, and is ignored. ``no.such.key`` is allow-listed: ``Game::languageSelfTest``
looks it up on purpose to prove an unknown key falls back rather than blanking.

Run with:  python3 tests/test_i18n_keys.py      (no third-party deps)
"""

import json
import os
import re
import sys
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(REPO_ROOT, "src")
LANG_DIR = os.path.join(REPO_ROOT, "assets", "lang")
EN_JSON = os.path.join(LANG_DIR, "en.json")
MISSIONS_DIR = os.path.join(REPO_ROOT, "assets", "data", "missions")
WEAPONS_JSON = os.path.join(REPO_ROOT, "assets", "data", "weapons.json")

# A mission id is the file stem; `missionIdFromPath` rejects anything that is not
# a safe key segment, so a mission whose stem does not match this yields no key
# and localisation is skipped — which is exactly what the guard must catch.
MISSION_ID = re.compile(r"^[a-z][a-z0-9_]*$")

# A weapon id is a catalogue `id`; `weaponNameKey` uses the same safe-segment
# rule as a mission id (lower-case, digits, `_`, `-`), so the same guard applies.
WEAPON_ID = re.compile(r"^[a-z][a-z0-9_-]*$")

# The shape `tr()` keys use: dotted, lower-camel segments.
KEY_SHAPED = re.compile(r'"([a-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)"')

# A literal whose last segment is one of these is a file name, not a key.
FILE_EXTENSIONS = {
    "json", "bmp", "txt", "conf", "cfg", "ini", "log", "md", "png",
    "ttf", "wav", "ogg", "dll", "exe", "so", "dylib",
}

# Keys looked up on purpose that are intentionally absent from the table.
ALLOWED_ABSENT = {"no.such.key"}

# `tr()` fallbacks known to disagree with `en.json`, pending a fix in the owning
# area. Each entry is `(key, source file)`. Keep this empty when possible; the
# comment names the issue that will remove the entry. This list is deliberately
# NOT auto-rotting: a stale entry must not fail another area's PR (the fix lands
# in a file this guard does not own), so review removes it by hand.
ALLOWED_FALLBACK_DRIFT: set = set()

# The leading `tr("key",` of a call. The fallback is captured separately so a
# `)` inside a literal (e.g. "({x},{y})") cannot end the call early.
TR_CALL = re.compile(r'\btr\(\s*"([a-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)"\s*,')

# A `//` line comment can mention a key; strip comments so documentation does
# not register as a real lookup.
LINE_COMMENT = re.compile(r"//[^\n]*")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def english_keys():
    with open(EN_JSON, encoding="utf-8") as handle:
        root = json.load(handle)
    return {k for k in root if not k.startswith("_")}


def english_table():
    """The ``key -> value`` map from ``en.json`` (ignoring ``_`` documentation)."""
    with open(EN_JSON, encoding="utf-8") as handle:
        root = json.load(handle)
    return {k: v for k, v in root.items() if not k.startswith("_")}


def load_table(path):
    """Return ``(keys, duplicate_keys)`` for a locale JSON object.

    A duplicate key (a translator copy-paste that shadows an earlier entry) is
    invisible to a normal ``json.load`` — the last value silently wins — so it is
    collected here via ``object_pairs_hook`` and reported.
    """
    duplicates = []

    def hook(pairs):
        seen = set()
        for key, _value in pairs:
            if key in seen:
                duplicates.append(key)
            seen.add(key)
        return dict(pairs)

    with open(path, encoding="utf-8") as handle:
        root = json.load(handle, object_pairs_hook=hook)
    keys = {k for k in root if not k.startswith("_")}
    dupes = [d for d in duplicates if not d.startswith("_")]
    return keys, dupes


def shipped_locale_files():
    """Every ``assets/lang/*.json`` except the English reference, sorted."""
    names = sorted(
        name for name in os.listdir(LANG_DIR)
        if name.endswith(".json") and name != "en.json"
    )
    return [os.path.join(LANG_DIR, name) for name in names]


def shipped_mission_ids():
    """The stem of every ``assets/data/missions/*.json``, sorted.

    The mission id is the file stem (``core::i18n::missionIdFromPath``), so this
    is the authoritative mission set — a new mission JSON cannot be forgotten by
    a hardcoded list.
    """
    names = sorted(
        name for name in os.listdir(MISSIONS_DIR) if name.endswith(".json")
    )
    return [name[: -len(".json")] for name in names]


def shipped_weapon_ids():
    """The ``id`` of every entry in ``assets/data/weapons.json``, sorted.

    The id is what ``core::i18n::weaponNameKey`` derives the localisation key
    from, so the catalogue file — not a hardcoded list — is the authoritative
    weapon set: a new weapon cannot be forgotten by a stale list (issue #534).
    """
    with open(WEAPONS_JSON, encoding="utf-8") as handle:
        root = json.load(handle)
    weapons = root.get("weapons", []) if isinstance(root, dict) else []
    return sorted(entry["id"] for entry in weapons if isinstance(entry, dict) and "id" in entry)


def source_files():
    for dirpath, _dirnames, filenames in os.walk(SRC_DIR):
        for name in sorted(filenames):
            if name.endswith((".cpp", ".h", ".in")):
                yield os.path.join(dirpath, name)


def key_literals():
    """Every key-shaped literal in ``src/`` -> sorted list of (key, file)."""
    found = []
    for path in source_files():
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        text = BLOCK_COMMENT.sub("", LINE_COMMENT.sub("", text))
        for match in KEY_SHAPED.finditer(text):
            literal = match.group(1)
            if literal.rsplit(".", 1)[-1] in FILE_EXTENSIONS:
                continue
            rel = os.path.relpath(path, REPO_ROOT)
            found.append((literal, rel))
    return found


def _decode_string(text, pos):
    """Decode the string literal that starts at ``pos`` (a quote). -> (value, end)."""
    assert text[pos] == '"'
    pos += 1
    chars = []
    escapes = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}
    while pos < len(text) and text[pos] != '"':
        if text[pos] == "\\" and pos + 1 < len(text):
            chars.append(escapes.get(text[pos + 1], text[pos + 1]))
            pos += 2
        else:
            chars.append(text[pos])
            pos += 1
    return "".join(chars), pos + 1


def tr_call_sites():
    """Every ``tr("key", "fallback")`` call -> list of (key, file, fallback).

    Only calls whose fallback is a plain double-quoted literal are captured (a
    variable or macro fallback is skipped). Adjacent string literals concatenate
    in C++, so ``tr("k", "a" "b")`` is joined into ``"ab"``.
    """
    sites = []
    for path in source_files():
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        text = BLOCK_COMMENT.sub("", LINE_COMMENT.sub("", text))
        rel = os.path.relpath(path, REPO_ROOT)
        for match in TR_CALL.finditer(text):
            key = match.group(1)
            pos = match.end()
            parts = []
            while True:
                while pos < len(text) and text[pos] in " \t\r\n":
                    pos += 1
                if pos >= len(text) or text[pos] != '"':
                    break
                value, pos = _decode_string(text, pos)
                parts.append(value)
            if parts:
                sites.append((key, rel, "".join(parts)))
    return sites


def _match_paren(text, pos):
    """Index of the ``)`` matching the ``(`` at ``pos``, or ``-1``.

    String literals are skipped, so a ``)`` inside one (e.g. ``"({x},{y})"``)
    does not close the call early.
    """
    depth = 0
    while pos < len(text):
        char = text[pos]
        if char == '"':
            _value, pos = _decode_string(text, pos)
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return pos
        pos += 1
    return -1


# The leading `substitute(tr("key",` / `substitute(lang_.get("key",` of a call.
SUBSTITUTE_CALL = re.compile(r"\bsubstitute\s*\(")
SUBSTITUTE_KEY = re.compile(
    r'\b(?:tr|lang_\.get)\(\s*"([a-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)"\s*,'
)
# One `{"name", value}` pair in the initializer list, e.g. `{"count", ...}`.
SUBSTITUTE_VALUE = re.compile(r'\{\s*"([a-z][a-z0-9_]*)"\s*,')
# A `{token}` in a format string.
PLACEHOLDER = re.compile(r"\{([a-z][a-z0-9_]*)\}")


def substitute_call_sites():
    """Every ``substitute(<key lookup>, {{"name", ...}})`` -> (key, file, names).

    Only calls whose first argument is a ``tr("key", ...)`` or
    ``lang_.get("key", ...)`` are captured; the ``substitute`` *definition* in
    ``HudStyle.h`` and any call that does not name a table key are skipped.
    ``names`` is the set of ``{"name", ...}`` keys in the initializer list — the
    values the call supplies for the format string's ``{token}`` s.
    """
    sites = []
    for path in source_files():
        with open(path, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        text = BLOCK_COMMENT.sub("", LINE_COMMENT.sub("", text))
        rel = os.path.relpath(path, REPO_ROOT)
        for match in SUBSTITUTE_CALL.finditer(text):
            open_paren = text.index("(", match.start())
            close_paren = _match_paren(text, open_paren)
            if close_paren == -1:
                continue
            block = text[open_paren:close_paren + 1]
            key_match = SUBSTITUTE_KEY.search(block)
            if key_match is None:
                continue
            provided = set(SUBSTITUTE_VALUE.findall(block))
            sites.append((key_match.group(1), rel, provided))
    return sites


class StringTableKeyCoverage(unittest.TestCase):
    def test_every_key_literal_resolves_in_en_json(self):
        keys = english_keys()
        self.assertTrue(keys, "en.json carried no keys")

        missing = {}
        for literal, rel in key_literals():
            if literal in keys or literal in ALLOWED_ABSENT:
                continue
            missing.setdefault(literal, set()).add(rel)

        if missing:
            lines = ["key-shaped literal(s) not present in assets/lang/en.json:"]
            for literal in sorted(missing):
                lines.append(f"  {literal}  ({', '.join(sorted(missing[literal]))})")
            lines.append(
                "Add the key to en.json + every locale, or allow-list it here if "
                "it is not a table lookup."
            )
            self.fail("\n".join(lines))

    def test_scanner_actually_finds_known_keys(self):
        # Guards against the regex silently matching nothing (a guard that can
        # never fail is worse than none).
        literals = {literal for literal, _rel in key_literals()}
        self.assertIn("hud.squad", literals)
        self.assertIn("app.title", literals)

    def test_tr_fallbacks_match_en_json(self):
        # A table edit (e.g. #210 adding "SPACE  PAUSE") must not leave a stale
        # `tr()` fallback behind: the fallback is the no-localisation copy, so it
        # has to read the same as en.json.
        table = english_table()
        self.assertTrue(table, "en.json carried no keys")

        sites = tr_call_sites()
        self.assertTrue(sites, "no tr(key, \"literal\") call sites found")

        problems = []
        for key, rel, fallback in sites:
            if key not in table or (key, rel) in ALLOWED_FALLBACK_DRIFT:
                continue
            if fallback != table[key]:
                problems.append(
                    f"  {rel}: {key}\n    fallback: {fallback!r}\n    en.json : {table[key]!r}"
                )

        if problems:
            self.fail(
                "tr() fallback(s) do not match en.json:\n"
                + "\n".join(problems)
                + "\nSync the fallback to en.json, or allow-list it in "
                "ALLOWED_FALLBACK_DRIFT with the issue that will fix it."
            )

    def test_every_locale_covers_the_english_key_set(self):
        # A locale must carry exactly the English keys: a missing key silently
        # shows English, and an invented key is dead weight (or a typo that the
        # UI would render raw). This is the per-language version of #204.
        english = english_keys()
        self.assertTrue(english, "en.json carried no keys")

        locales = shipped_locale_files()
        self.assertTrue(locales, "no shipped locale files found")

        problems = []
        for path in locales:
            keys, dupes = load_table(path)
            missing = sorted(english - keys)
            extra = sorted(keys - english)
            if missing or extra or dupes:
                detail = os.path.basename(path)
                if missing:
                    detail += " | missing: " + ", ".join(missing)
                if extra:
                    detail += " | extra: " + ", ".join(extra)
                if dupes:
                    detail += " | duplicate: " + ", ".join(sorted(set(dupes)))
                problems.append(detail)

        if problems:
            self.fail(
                "locale key set(s) do not match en.json:\n  "
                + "\n  ".join(problems)
                + "\nAdd the missing keys (translated), remove any extras, and "
                "de-duplicate repeated keys."
            )

    def test_en_json_has_no_duplicate_keys(self):
        # The English reference is loaded by name in most guards, so a duplicate
        # key there is the one place the duplicate check above does not reach.
        _keys, dupes = load_table(EN_JSON)
        if dupes:
            self.fail(
                "en.json has duplicate key(s): " + ", ".join(sorted(set(dupes)))
            )

    def test_every_mission_has_localised_name_and_desc(self):
        # Mission name/description keys are derived at runtime from the mission
        # file stem (`mission.<id>.name` / `mission.<id>.desc`), so they are not
        # key-shaped literals in `src/` and rule 1 cannot see them. Every shipped
        # mission must be localisable in every locale, or a new mission ships a
        # raw key / blank title (the silent-English class of #191).
        english = english_table()
        self.assertTrue(english, "en.json carried no keys")

        ids = shipped_mission_ids()
        self.assertTrue(ids, "no mission files found")

        locale_tables = {}
        for path in shipped_locale_files():
            keys, _dupes = load_table(path)
            locale_tables[os.path.basename(path)] = keys

        problems = []
        for mission_id in ids:
            if not MISSION_ID.match(mission_id):
                problems.append(
                    f"  {mission_id}.json: id is not a safe key segment "
                    f"(localisation would be skipped)"
                )
                continue
            for suffix in ("name", "desc"):
                key = f"mission.{mission_id}.{suffix}"
                if key not in english:
                    problems.append(f"  en.json: missing {key}")
                for locale, keys in sorted(locale_tables.items()):
                    if key not in keys:
                        problems.append(f"  {locale}: missing {key}")

        if problems:
            self.fail(
                "mission localisation is incomplete:\n"
                + "\n".join(problems)
                + "\nAdd `mission.<id>.name` and `mission.<id>.desc` to en.json "
                "and every locale."
            )

    def test_every_weapon_has_a_localised_name(self):
        # Weapon name keys are derived at runtime from the catalogue id
        # (`weapon.<id>.name`, `core::i18n::weaponNameKey`, issue #534), so they
        # are not key-shaped literals in `src/` and rule 1 cannot see them. Every
        # shipped weapon must be localisable in every locale, or a localised run
        # silently shows the authored English name (the silent-English class of
        # #191).
        english = english_table()
        self.assertTrue(english, "en.json carried no keys")

        ids = shipped_weapon_ids()
        self.assertTrue(ids, "no weapon entries found")

        locale_tables = {}
        for path in shipped_locale_files():
            keys, _dupes = load_table(path)
            locale_tables[os.path.basename(path)] = keys

        problems = []
        for weapon_id in ids:
            if not WEAPON_ID.match(weapon_id):
                problems.append(
                    f"  {weapon_id}: id is not a safe key segment "
                    f"(localisation would be skipped)"
                )
                continue
            key = f"weapon.{weapon_id}.name"
            if key not in english:
                problems.append(f"  en.json: missing {key}")
            for locale, keys in sorted(locale_tables.items()):
                if key not in keys:
                    problems.append(f"  {locale}: missing {key}")

        if problems:
            self.fail(
                "weapon localisation is incomplete:\n"
                + "\n".join(problems)
                + "\nAdd `weapon.<id>.name` to en.json and every locale."
            )

    def test_no_locale_value_is_empty(self):
        # An empty value is NOT "untranslated": StringTable::get returns a
        # present key's value verbatim, so "" wins over the tr() fallback and
        # blanks the UI element. The mission path already treats empty as
        # untranslated; the UI path does not, and nothing else guards this.
        problems = []
        for path in [EN_JSON] + shipped_locale_files():
            with open(path, encoding="utf-8") as handle:
                root = json.load(handle)
            for key, value in root.items():
                if key.startswith("_"):
                    continue
                if not isinstance(value, str) or value.strip() == "":
                    problems.append(f"  {os.path.basename(path)}: {key}")
        if problems:
            self.fail(
                "empty string value(s) would blank a UI element:\n"
                + "\n".join(problems)
                + "\nGive every key a non-empty value."
            )

    def test_english_values_are_renderable(self):
        # BitmapFont::draw maps any byte outside ASCII 32..126 to a blank glyph
        # (src/game/Font.cpp). English values are drawn by that same font, some
        # with no tr() fallback to hide behind (the window title, and
        # mission.<id>.name / .desc), so a non-ASCII English string ships an
        # invisible glyph. The C++ renderability gate only walks the locale
        # files, so the reference table itself was unchecked (#288).
        problems = []
        with open(EN_JSON, encoding="utf-8") as handle:
            root = json.load(handle)
        for key, value in root.items():
            if key.startswith("_"):
                continue
            if not isinstance(value, str):
                continue
            bad = sorted({ch for ch in value if not 32 <= ord(ch) <= 126})
            if bad:
                shown = ", ".join(f"{ch!r} (U+{ord(ch):04X})" for ch in bad)
                problems.append(f"  {key}: {shown}")
        if problems:
            self.fail(
                "en.json value(s) use a character the embedded font cannot "
                "draw (it renders blank):\n"
                + "\n".join(problems)
                + "\nThe 5x7 bitmap font covers ASCII 32..126 only; use an ASCII "
                "transliteration (e.g. '-' for an em-dash)."
            )

    def test_every_substitute_site_supplies_its_placeholders(self):
        # `hudstyle::substitute` replaces each `{token}` it is given a value for
        # and leaves the rest verbatim (src/game/HudStyle.h), so a call site that
        # supplies fewer names than the format string has tokens draws a raw
        # `{count}` / `{x}` on the HUD. Nothing else ties the two together: rule 3
        # keeps the tr() fallback equal to en.json (so a format-vs-argument
        # mismatch is invisible there), the C++ gate only compares a locale's
        # placeholder set to en.json, and Game::objectiveSelfTest checks the
        # objective banner alone at runtime. This is the cheap editor-side half of
        # that invariant.
        table = english_table()
        self.assertTrue(table, "en.json carried no keys")

        sites = substitute_call_sites()
        self.assertTrue(sites, "no substitute(<key lookup>, {...}) call sites found")
        # Guard against the scanner degenerating to the trivial single-name shape:
        # hud.agentRow is the widest shipped format string (four tokens).
        agent_row = [provided for key, _rel, provided in sites if key == "hud.agentRow"]
        self.assertTrue(agent_row, "scanner missed the hud.agentRow call site")
        self.assertTrue(
            all(provided == {"name", "marker", "hp", "weapon"} for provided in agent_row),
            f"scanner mis-parsed the hud.agentRow names: {agent_row}",
        )

        problems = []
        for key, rel, provided in sites:
            if key not in table:
                if key in ALLOWED_ABSENT:
                    continue
                problems.append(f"  {rel}: {key}  (not in en.json)")
                continue
            wanted = set(PLACEHOLDER.findall(table[key]))
            missing = wanted - provided
            extra = provided - wanted
            if missing or extra:
                detail = []
                if missing:
                    detail.append(f"missing {sorted(missing)} (would be drawn raw)")
                if extra:
                    detail.append(f"unused {sorted(extra)}")
                problems.append(f"  {rel}: {key}  ({'; '.join(detail)})")

        if problems:
            self.fail(
                "substitute() call site(s) do not match their format string's "
                "placeholders:\n"
                + "\n".join(problems)
                + "\nSupply exactly the {tokens} the format string in en.json uses."
            )


if __name__ == "__main__":
    unittest.main(verbosity=2)
