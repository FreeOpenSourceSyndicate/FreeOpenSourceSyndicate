#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
"""String-table key coverage guard (issue #191).

The UI reads its strings from the table in ``assets/lang/en.json`` via
``tr(key, fallback)``. If the engine asks for a key the table does not carry,
the literal fallback is shown — so a localised run silently shows English (the
exact "stays English in a `de` run" failure #191 is about). This test makes that
impossible to merge.

Rule
----
Every **key-shaped string literal** in ``src/`` must resolve in ``en.json``.

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
EN_JSON = os.path.join(REPO_ROOT, "assets", "lang", "en.json")

# The shape `tr()` keys use: dotted, lower-camel segments.
KEY_SHAPED = re.compile(r'"([a-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+)+)"')

# A literal whose last segment is one of these is a file name, not a key.
FILE_EXTENSIONS = {
    "json", "bmp", "txt", "conf", "cfg", "ini", "log", "md", "png",
    "ttf", "wav", "ogg", "dll", "exe", "so", "dylib",
}

# Keys looked up on purpose that are intentionally absent from the table.
ALLOWED_ABSENT = {"no.such.key"}

# A `//` line comment can mention a key; strip comments so documentation does
# not register as a real lookup.
LINE_COMMENT = re.compile(r"//[^\n]*")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def english_keys():
    with open(EN_JSON, encoding="utf-8") as handle:
        root = json.load(handle)
    return {k for k in root if not k.startswith("_")}


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
