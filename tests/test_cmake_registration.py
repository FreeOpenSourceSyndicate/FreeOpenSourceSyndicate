#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
"""CMake registration guard (issue #298).

Source and test registration is **manual**: ``src/core/CMakeLists.txt``,
``src/CMakeLists.txt`` and ``tests/CMakeLists.txt`` list every ``.cpp``
explicitly (no globbing). Nothing checked those lists against the tree, so a file
that is added but **not registered** silently never compiles, its tests never
run, and CI stays green — the author gets false confidence their tests passed.
This is the silent-omission class of #103/#126/#283, one level down: there the
file never *shipped*, here it never *reaches the build*.

Targets checked
---------------
1. ``FOSSYND_CORE_SOURCES`` (``src/core/CMakeLists.txt``) vs every ``.cpp``
   under ``src/core/``.
2. ``FOSSYND_GAME_SOURCES`` (``src/CMakeLists.txt``) vs ``src/main.cpp`` and
   every ``.cpp`` under ``src/game/``.
3. ``FOSSYND_TEST_SOURCES`` (``tests/CMakeLists.txt``) vs every ``.cpp`` under
   ``tests/``.
4. Every ``tests/test_*.py`` vs the ``add_test(...)`` targets in
   ``tests/CMakeLists.txt`` (issue #386). The Python suite is registered by hand
   as separate ctest targets, so a new ``test_*.py`` that is written but not
   wired up never runs - invisible to ctest, green CI, false confidence. This is
   the same silent-omission class as 1-3, one level down.

Generated sources (``${CMAKE_CURRENT_BINARY_DIR}/...``, appended to the game
list via ``list(APPEND ...)``) do not exist on disk, so they are ignored on both
sides.

Run with:  python3 tests/test_cmake_registration.py     (no third-party deps)
"""

import os
import re
import sys
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# A `#` line comment must not count as a registration (a commented-out source is
# not built), so strip them before parsing the list body.
CMAKELIST_COMMENT = re.compile(r"#[^\n]*")

# A concrete `.cpp` entry. `${...}`-prefixed (generated) entries are skipped.
CPP_ENTRY = re.compile(r"(?<![A-Za-z0-9_/${.])[A-Za-z0-9_][A-Za-z0-9_/.-]*\.cpp\b")


def cmake_list(cmake_path, list_name):
    """The ``.cpp`` entries registered in ``set(<list_name> ...)`` -> set of rels.

    Relative to the directory that holds ``cmake_path`` (the form the list uses).
    Generated ``${CMAKE_CURRENT_BINARY_DIR}/...`` entries are excluded: they are
    not on disk.
    """
    with open(cmake_path, encoding="utf-8") as handle:
        text = CMAKELIST_COMMENT.sub("", handle.read())
    match = re.search(
        r"set\(\s*" + re.escape(list_name) + r"\s+(.*?)\)", text, re.DOTALL
    )
    if match is None:
        raise AssertionError(f"{cmake_path}: no set({list_name} ...) block found")
    body = match.group(1)
    return {
        entry
        for entry in CPP_ENTRY.findall(body)
        if "${" not in entry
    }


def on_disk_cpp(base_dir, roots):
    """Every ``.cpp`` under ``base_dir`` within ``roots`` -> set of rel paths.

    ``roots`` are paths relative to ``base_dir``; each is either a file (itself)
    or a directory (walked recursively). Paths are returned in the same
    ``/``-separated, base-relative form the CMake lists use.
    """
    found = set()
    for root in roots:
        path = os.path.join(base_dir, root)
        if os.path.isfile(path) and path.endswith(".cpp"):
            found.add(root)
        elif os.path.isdir(path):
            for dirpath, _dirnames, filenames in os.walk(path):
                for name in filenames:
                    if name.endswith(".cpp"):
                        full = os.path.join(dirpath, name)
                        found.add(os.path.relpath(full, base_dir).replace(os.sep, "/"))
    return found


# (label, CMakeLists path, on-disk base dir, roots relative to that base)
TARGETS = [
    (
        "FOSSYND_CORE_SOURCES",
        os.path.join(REPO_ROOT, "src", "core", "CMakeLists.txt"),
        os.path.join(REPO_ROOT, "src", "core"),
        ["."],
    ),
    (
        "FOSSYND_GAME_SOURCES",
        os.path.join(REPO_ROOT, "src", "CMakeLists.txt"),
        os.path.join(REPO_ROOT, "src"),
        ["main.cpp", "game"],
    ),
    (
        "FOSSYND_TEST_SOURCES",
        os.path.join(REPO_ROOT, "tests", "CMakeLists.txt"),
        os.path.join(REPO_ROOT, "tests"),
        ["."],
    ),
]


def drift_for(list_name, cmake_path, base_dir, roots):
    """Return ``(unregistered, missing)`` for one target.

    ``unregistered`` = on disk but absent from the list (silently never built);
    ``missing`` = listed but absent from disk (a stale entry, or a rename).
    """
    registered = cmake_list(cmake_path, list_name)
    present = on_disk_cpp(base_dir, roots)
    return sorted(present - registered), sorted(registered - present)


# `add_test(NAME <name> COMMAND ...)` in tests/CMakeLists.txt. Only the COMMAND
# matters here: the Python targets invoke `${Python3_EXECUTABLE} <path>`.
ADD_TEST = re.compile(r"add_test\s*\(\s*NAME\s+(\S+)\s+COMMAND\s+([^\n)]*)", re.IGNORECASE)

# A `tests/test_*.py` file, the naming convention the Python suite follows.
PY_TEST_NAME = re.compile(r"^test_[A-Za-z0-9_]*\.py$")


def registered_python_tests(cmake_path):
    """Every ``tests/*.py`` named by an ``add_test`` COMMAND -> set of basenames.

    The path is taken from the COMMAND line and reduced to its basename, so it
    matches however the target spells the directory (``${CMAKE_SOURCE_DIR}/tests``,
    an absolute path, or a bare relative one).
    """
    with open(cmake_path, encoding="utf-8") as handle:
        text = CMAKELIST_COMMENT.sub("", handle.read())
    names = set()
    for _name, command in ADD_TEST.findall(text):
        for token in command.split():
            base = os.path.basename(token)
            if base.endswith(".py"):
                names.add(base)
    return names


def python_test_drift(cmake_path, tests_dir):
    """``(unregistered, missing)`` for the ``tests/test_*.py`` suite.

    ``unregistered`` = a ``tests/test_*.py`` on disk that no ``add_test`` runs;
    ``missing`` = an ``add_test`` naming a ``tests/*.py`` that is not on disk.
    """
    present = {
        name
        for name in os.listdir(tests_dir)
        if os.path.isfile(os.path.join(tests_dir, name)) and PY_TEST_NAME.match(name)
    }
    registered = registered_python_tests(cmake_path)
    return sorted(present - registered), sorted(registered - present)


class CMakeRegistration(unittest.TestCase):
    def test_every_source_and_test_is_registered(self):
        problems = []
        for list_name, cmake_path, base_dir, roots in TARGETS:
            unregistered, missing = drift_for(list_name, cmake_path, base_dir, roots)
            rel_cmake = os.path.relpath(cmake_path, REPO_ROOT)
            for entry in unregistered:
                problems.append(
                    f"  {rel_cmake}: {list_name} does not list `{entry}` "
                    f"(on disk but never built/tested)"
                )
            for entry in missing:
                problems.append(
                    f"  {rel_cmake}: {list_name} lists `{entry}` "
                    f"(not on disk: rename it or drop the entry)"
                )

        if problems:
            self.fail(
                "CMake registration drift:\n"
                + "\n".join(problems)
                + "\nRegister the file in its `set(FOSSYND_*_SOURCES ...)` list "
                "(or remove the stale entry). A file that is not listed is "
                "silently never built and its tests never run."
            )

    def test_every_python_test_is_registered(self):
        tests_cmake = TARGETS[2][1]
        tests_dir = os.path.dirname(tests_cmake)
        unregistered, missing = python_test_drift(tests_cmake, tests_dir)
        rel_cmake = os.path.relpath(tests_cmake, REPO_ROOT)
        problems = [
            f"  {rel_cmake}: `{entry}` is on disk but no `add_test` runs it "
            f"(it never runs in ctest, so CI is green and the author believes "
            f"it passed)"
            for entry in unregistered
        ] + [
            f"  {rel_cmake}: an `add_test` names `{entry}`, which is not on disk "
            f"(rename it or drop the target)"
            for entry in missing
        ]
        if problems:
            self.fail(
                "Python test registration drift:\n"
                + "\n".join(problems)
                + "\nAdd an `add_test(NAME <name> COMMAND ${Python3_EXECUTABLE} "
                "tests/<file>)` target in tests/CMakeLists.txt (or remove the "
                "stale one). A test that is not registered never runs."
            )

    def test_parser_actually_reads_known_entries(self):
        # A guard that can never fail is worse than none: prove the parser sees
        # real entries and the on-disk scan sees real files before trusting the
        # comparison above.
        core = cmake_list(TARGETS[0][1], "FOSSYND_CORE_SOURCES")
        self.assertIn("data/Json.cpp", core)
        self.assertIn("math/Isometric.cpp", core)

        tests = cmake_list(TARGETS[2][1], "FOSSYND_TEST_SOURCES")
        self.assertIn("main.cpp", tests)
        self.assertIn("test_agent.cpp", tests)

        game_present = on_disk_cpp(TARGETS[1][2], TARGETS[1][3])
        self.assertIn("main.cpp", game_present)
        self.assertIn("game/Game.cpp", game_present)

        # And the Python side sees the real add_test targets and files.
        registered_py = registered_python_tests(TARGETS[2][1])
        self.assertIn("test_cmake_registration.py", registered_py)
        self.assertIn("test_coord.py", registered_py)
        present_py = {
            name
            for name in os.listdir(os.path.dirname(TARGETS[2][1]))
            if PY_TEST_NAME.match(name)
        }
        self.assertIn("test_coord.py", present_py)

    def test_python_test_drift_flags_an_unregistered_file(self):
        # Prove the new check actually bites: a `tests/test_*.py` with no
        # `add_test` target must be reported, not silently accepted.
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tests_dir = os.path.join(tmp, "tests")
            os.makedirs(tests_dir)
            cmake_path = os.path.join(tests_dir, "CMakeLists.txt")
            with open(cmake_path, "w", encoding="utf-8") as handle:
                handle.write(
                    "add_test(NAME wired\n"
                    "         COMMAND ${Python3_EXECUTABLE} "
                    "${CMAKE_SOURCE_DIR}/tests/test_wired.py)\n"
                )
            for name in ("test_wired.py", "test_orphan.py"):
                with open(os.path.join(tests_dir, name), "w", encoding="utf-8"):
                    pass
            unregistered, missing = python_test_drift(cmake_path, tests_dir)
            self.assertEqual(unregistered, ["test_orphan.py"])
            self.assertEqual(missing, [])

            # A target pointing at a vanished file is the mirror-image drift.
            os.remove(os.path.join(tests_dir, "test_wired.py"))
            unregistered, missing = python_test_drift(cmake_path, tests_dir)
            self.assertEqual(unregistered, ["test_orphan.py"])
            self.assertEqual(missing, ["test_wired.py"])

    def test_each_list_is_non_empty(self):
        # If a `set(...)` parse silently returned nothing, the comparison above
        # would flag every file as unregistered — but only if the list parsed at
        # all. Assert each list resolved to something.
        for list_name, cmake_path, _base_dir, _roots in TARGETS:
            self.assertTrue(
                cmake_list(cmake_path, list_name),
                f"{list_name} parsed as empty",
            )
        self.assertTrue(registered_python_tests(TARGETS[2][1]),
                        "no Python test targets parsed")


if __name__ == "__main__":
    unittest.main(verbosity=2)
