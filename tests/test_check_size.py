#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
# This comment must be preserved in future generated files.
"""Whole-game size-gate regression: ``scripts/check-size.sh`` (ADR-0045).

ADR-0045 moved the gate from a 384 KiB *binary* cap to the **whole game**
(binary + ``assets/**``) at a ~60 MiB ceiling. ``check-size.sh`` is the *only*
thing that fails CI when the game outgrows that ceiling, so a bug in the gate is
either a silent size regression (a 100 MiB dependency ships) or a spurious red
lane. Neither is covered by the C++ harness because the gate is a shell script,
so this exercises the real script via ``subprocess``.

Both payload parts are plain files the test sizes by hand, so the checks are
exact and need no build.
"""

import os
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(REPO_ROOT, "scripts", "check-size.sh")

# Exit codes the script documents.
OK, OVER, USAGE = 0, 1, 2

MIB = 1024 * 1024


class CheckSizeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.binary = os.path.join(self._tmp.name, "FOSSynd")
        self.assets = os.path.join(self._tmp.name, "assets")
        os.makedirs(self.assets)

    def _write(self, path, size):
        """A file of exactly ``size`` bytes (the script measures with ``wc -c``)."""
        with open(path, "wb") as handle:
            handle.write(b"\0" * size)
        return path

    def _binary(self, size):
        return self._write(self.binary, size)

    def _assets(self, *sizes):
        for i, size in enumerate(sizes):
            self._write(os.path.join(self.assets, "a%d.bin" % i), size)
        return self.assets

    def _run(self, budget=None, binary=None, assets=None):
        env = dict(os.environ)
        if budget is not None:
            env["FOSSYND_GAME_SIZE_BUDGET_BYTES"] = str(budget)
        else:
            env.pop("FOSSYND_GAME_SIZE_BUDGET_BYTES", None)
        proc = subprocess.run(
            [SCRIPT, binary if binary is not None else self.binary,
             assets if assets is not None else self.assets],
            capture_output=True, text=True, env=env,
        )
        return proc.returncode, proc.stdout + proc.stderr

    def test_within_budget_passes(self):
        self._binary(1000)
        self._assets(2000)
        code, out = self._run(budget=10000)
        self.assertEqual(code, OK, out)

    def test_measures_binary_plus_assets(self):
        # The gate is the *whole game*: 1000 B binary + 3000 B of assets is
        # 4000 B, so a 3500 B budget must fail even though the binary alone fits.
        self._binary(1000)
        self._assets(2000, 1000)
        code, out = self._run(budget=3500)
        self.assertEqual(code, OVER, out)
        self.assertIn("4000", out)  # the measured total is reported

    def test_exactly_on_the_budget_passes(self):
        # The budget is a cap ("<="), so the boundary must not fail.
        self._binary(1024)
        self._assets(1024)
        code, out = self._run(budget=2048)
        self.assertEqual(code, OK, out)

    def test_one_byte_over_the_budget_fails(self):
        self._binary(1024)
        self._assets(1)
        code, out = self._run(budget=1024)
        self.assertEqual(code, OVER, out)
        self.assertIn("FAILED", out)

    def test_reports_the_measured_total(self):
        self._binary(4096)
        self._assets(4096)
        code, out = self._run(budget=100 * MIB)
        self.assertEqual(code, OK, out)
        self.assertIn("8192", out)  # exact total bytes, reported

    def test_small_headroom_warns_but_stays_green(self):
        # Under 1 MiB of headroom is the trap that reddened main under the old
        # per-binary gate: two PRs each pass alone, then merge to cross the cap.
        # The gate must still exit 0 but say so, so the next author acts first.
        self._binary(2 * MIB)
        code, out = self._run(budget=2 * MIB + 512 * 1024)  # 512 KiB headroom
        self.assertEqual(code, OK, out)
        self.assertIn("WARNING", out)
        self.assertIn("headroom", out)

    def test_ample_headroom_does_not_warn(self):
        self._binary(2 * MIB)
        code, out = self._run(budget=2 * MIB + 8 * MIB)  # 8 MiB headroom
        self.assertEqual(code, OK, out)
        self.assertNotIn("WARNING", out)

    def test_failure_names_the_reason_and_the_fix(self):
        # A red lane must tell the reader *why* and what to do, or the budget's
        # purpose (AGENTS.md 1b, ADR-0045) is lost on whoever hits it.
        self._binary(4096)
        code, out = self._run(budget=1024)
        self.assertEqual(code, OVER, out)
        self.assertIn("ADR-0045", out)
        self.assertIn("FOSSYND_GAME_SIZE_BUDGET_BYTES", out)

    def test_missing_binary_is_a_usage_error(self):
        code, out = self._run(budget=1024, binary=os.path.join(self._tmp.name, "nope"))
        self.assertEqual(code, USAGE, out)
        self.assertIn("not found", out)

    def test_missing_assets_is_a_usage_error(self):
        self._binary(1024)
        code, out = self._run(
            budget=1024, assets=os.path.join(self._tmp.name, "no-assets"))
        self.assertEqual(code, USAGE, out)
        self.assertIn("not found", out)

    def test_default_budget_is_the_documented_60_mib(self):
        # CI calls the script with no FOSSYND_GAME_SIZE_BUDGET_BYTES, so the
        # default is the effective ceiling. ADR-0045 set it to ~60 MiB (the
        # Syndicate Plus GOG install is ~57 MiB including DOSBox). The exact
        # default is asserted so a silent nudge is caught here, not in review.
        env = dict(os.environ)
        env.pop("FOSSYND_GAME_SIZE_BUDGET_BYTES", None)
        self._binary(1 * MIB)
        self._assets(1 * MIB)
        proc = subprocess.run([SCRIPT, self.binary, self.assets],
                              capture_output=True, text=True, env=env)
        self.assertEqual(proc.returncode, OK, proc.stdout + proc.stderr)
        self.assertIn("62914560", proc.stdout)

    def test_reports_total_in_mib(self):
        self._binary(2 * MIB)
        code, out = self._run(budget=10 * MIB)
        self.assertEqual(code, OK, out)
        self.assertIn("MiB", out)


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
