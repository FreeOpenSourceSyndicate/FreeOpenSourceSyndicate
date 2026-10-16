#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
"""Packaging-gate regression: ``scripts/check-package-contents.sh`` (issue #372).

``check-package-contents.sh`` (issue #307) is the *only* thing that fails CI when
a shipped asset does not reach the Linux package: it derives the expected files
from the repo tree and the actual ones from the tarball, so a new mission, world,
locale, or shared catalogue is required automatically instead of needing a
hand-added ``grep -q``. That is what makes it the guard against the
#103/#126/#283 silent-omission class, and #343 (``vehicles.json``) relied on it.

It is a shell script the C++ harness never touches, and it has a precise
contract (exit 0 complete, 1 a file is missing, 2 usage error). A bug in it is
either a **silent** incomplete package or a **spurious** red packaging lane -
neither covered today. Like ``test_check_size.py`` (#328), this drives the real
script via ``subprocess`` against synthetic tarballs built in a temp dir, so the
checks are exact and need no build.

Run with:  python3 tests/test_check_package_contents.py    (no third-party deps)
"""

import os
import subprocess
import sys
import tarfile
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(REPO_ROOT, "scripts", "check-package-contents.sh")

# Exit codes the script documents.
OK, MISSING, USAGE = 0, 1, 2

# The prefix CPack puts around every entry; the script strips it before matching.
PREFIX = "FOSSynd-0.8.0-Linux"

# Distinguishes "caller wants the default tarball" from "pass no argument".
_DEFAULT = object()


def _glob(rel_dir, suffix=".json"):
    """Basenames of ``rel_dir/*suffix`` in the repo, sorted for determinism."""
    directory = os.path.join(REPO_ROOT, rel_dir)
    return sorted(f for f in os.listdir(directory) if f.endswith(suffix))


class CheckPackageContentsTest(unittest.TestCase):
    """Build a tarball from the *real* tree, then remove one file at a time."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tarball = os.path.join(self._tmp.name, "FOSSynd-0.8.0-Linux.tar.gz")
        # The exact set the script requires, derived the same way it derives it.
        self.entries = self._expected_entries()
        self._write_tarball(self.entries)

    def _expected_entries(self):
        """Every path the script requires, as it appears inside the tarball."""
        entries = [f"{PREFIX}/bin/FOSSynd"]
        for rel_dir, dest in (("assets/data/missions", "missions"),
                              ("assets/data/world", "world"),
                              ("assets/lang", "lang")):
            for name in _glob(rel_dir):
                entries.append(f"{PREFIX}/share/fossynd/{dest}/{name}")
        # Shared catalogues sit at the top of assets/data/ (e.g. vehicles.json).
        for name in _glob("assets/data"):
            entries.append(f"{PREFIX}/share/fossynd/{name}")
        entries += [
            f"{PREFIX}/share/fossynd/fossynd.conf.example",
            f"{PREFIX}/share/doc/fossynd/LICENSE",
            f"{PREFIX}/share/doc/fossynd/README.md",
        ]
        return entries

    def _write_tarball(self, entries):
        """A gzip tarball holding exactly ``entries`` (all empty files)."""
        with tarfile.open(self.tarball, "w:gz") as tar:
            for path in entries:
                info = tarfile.TarInfo(path)
                info.size = 0
                tar.addfile(info)

    def _run(self, tarball=_DEFAULT):
        """Run the script; ``tarball=None`` means pass no argument at all."""
        if tarball is _DEFAULT:
            tarball = self.tarball
        argv = [SCRIPT] if tarball is None else [SCRIPT, tarball]
        proc = subprocess.run(argv, capture_output=True, text=True)
        return proc.returncode, proc.stdout + proc.stderr

    def test_complete_package_passes(self):
        code, out = self._run()
        self.assertEqual(code, OK, out)
        self.assertIn("package check OK", out)

    def test_missing_binary_fails_and_names_it(self):
        self._write_tarball([e for e in self.entries if not e.endswith("/bin/FOSSynd")])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn("bin/FOSSynd", out)

    def test_missing_mission_fails(self):
        victim = next(e for e in self.entries if "/missions/" in e)
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn(os.path.basename(victim), out)

    def test_missing_world_fails(self):
        victim = next(e for e in self.entries if "/world/" in e)
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn(os.path.basename(victim), out)

    def test_missing_locale_fails(self):
        victim = next(e for e in self.entries if "/lang/" in e)
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn(os.path.basename(victim), out)

    def test_missing_shared_catalogue_fails(self):
        # The #343 regression class: a catalogue at the top of assets/data/ that
        # is added to the tree but not to the install() list.
        catalogues = _glob("assets/data")
        self.assertTrue(catalogues, "expected at least one shared catalogue")
        victim = f"{PREFIX}/share/fossynd/{catalogues[0]}"
        self.assertIn(victim, self.entries)
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn(catalogues[0], out)

    def test_missing_config_example_fails(self):
        victim = f"{PREFIX}/share/fossynd/fossynd.conf.example"
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn("fossynd.conf.example", out)

    def test_missing_license_fails(self):
        victim = f"{PREFIX}/share/doc/fossynd/LICENSE"
        self._write_tarball([e for e in self.entries if e != victim])
        code, out = self._run()
        self.assertEqual(code, MISSING, out)
        self.assertIn("LICENSE", out)

    def test_no_argument_is_a_usage_error(self):
        code, out = self._run(tarball=None)
        self.assertEqual(code, USAGE, out)
        self.assertIn("usage", out)

    def test_nonexistent_tarball_is_a_usage_error(self):
        code, out = self._run(tarball=os.path.join(self._tmp.name, "nope.tar.gz"))
        self.assertEqual(code, USAGE, out)
        self.assertIn("not found", out)

    def test_corrupt_tarball_does_not_pass(self):
        # A tarball that cannot be listed must fail loudly, not pass with an
        # empty listing (which would make every `require` miss and - if the exit
        # code were swallowed - look like a spurious pass).
        with open(self.tarball, "wb") as handle:
            handle.write(b"this is not a gzip tarball\n")
        code, _ = self._run()
        self.assertNotEqual(code, OK)


if __name__ == "__main__":
    unittest.main()
