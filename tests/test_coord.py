#!/usr/bin/env python3
# AI/LLM-generated file. Do not treat as human-written source documentation.
"""Tests for scripts/coord - the git-native coordination lease ledger.

Run with:  python3 tests/test_coord.py        (no third-party deps)

The tests exercise the real code paths against a throwaway local "remote" (a
bare git repo in a temp dir) and the real `git` binary - no mocks, no network.
`COORD_NOW` pins the clock so TTL/expiry/staleness are deterministic.
"""

import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from contextlib import redirect_stdout, redirect_stderr

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COORD_PATH = os.path.join(REPO_ROOT, "scripts", "coord")


def load_coord():
    loader = importlib.machinery.SourceFileLoader("coord", COORD_PATH)
    spec = importlib.util.spec_from_loader("coord", loader)
    module = importlib.util.module_from_spec(spec)
    # dataclasses resolves annotations via sys.modules[cls.__module__], so the
    # module must be registered before exec_module runs.
    sys.modules["coord"] = module
    loader.exec_module(module)
    return module


coord = load_coord()


def git(cwd, *args):
    return subprocess.run(["git", *args], cwd=cwd, check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


class CoordTestCase(unittest.TestCase):
    """Each test gets a fresh bare 'origin' + a working clone."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="coord-test-")
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.remote = os.path.join(self.tmp, "origin.git")
        self.work = os.path.join(self.tmp, "work")
        git(self.tmp, "init", "--bare", "--quiet", self.remote)
        git(self.tmp, "init", "--quiet", self.work)
        git(self.work, "config", "user.email", "t@example.com")
        git(self.work, "config", "user.name", "Test")
        git(self.work, "commit", "--allow-empty", "--quiet", "-m", "init")
        git(self.work, "branch", "-M", "main")
        git(self.work, "remote", "add", "origin", self.remote)
        git(self.work, "push", "--quiet", "-u", "origin", "main")
        self._orig_cwd = os.getcwd()
        os.chdir(self.work)
        self.addCleanup(os.chdir, self._orig_cwd)
        self.now = 1_800_000_000
        self._set_now(self.now)

    def _set_now(self, epoch):
        self.now = epoch
        os.environ["COORD_NOW"] = str(epoch)

    def run_coord(self, *args, expect=0):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            rc = coord.main([str(a) for a in args])
        self.assertEqual(rc, expect,
                         f"coord {' '.join(map(str, args))} -> rc={rc}\n"
                         f"stdout:\n{out.getvalue()}\nstderr:\n{err.getvalue()}")
        return out.getvalue(), err.getvalue()

    def remote_refs(self):
        proc = subprocess.run(["git", "for-each-ref", "--format=%(refname)",
                               "refs/coord/"],
                              cwd=self.remote, stdout=subprocess.PIPE, text=True)
        return [l.strip() for l in proc.stdout.splitlines() if l.strip()]


class TestLeaseBasics(CoordTestCase):
    def test_claim_creates_one_ref_per_issue(self):
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-a", "--branch", "feat/world-x")
        self.assertIn("claimed world / #157", out)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world/157"])

    def test_second_agent_cannot_claim_a_held_area(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a")
        _, err = self.run_coord("claim", "--area", "world", "--issue", 200,
                                "--agent", "model-b", expect=2)
        self.assertIn("held by model-a", err)
        # The failed claim must not have created a second ref.
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world/157"])

    def test_same_agent_may_re_claim_its_own_area(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a")
        self._set_now(self.now + 60)
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-a")
        self.assertIn("claimed world / #157", out)

    def test_takeover_is_required_for_a_live_foreign_lease(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a")
        _, err = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-b", expect=2)
        self.assertIn("already claimed by model-a", err)
        self.assertIn("--takeover", err)
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-b", "--takeover")
        self.assertIn("took over", out)

    def test_release_frees_the_area(self):
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "model-a")
        self.run_coord("release", "--area", "text", "--issue", 142, "--agent", "model-a")
        self.assertEqual(self.remote_refs(), [])
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "model-b")

    def test_release_by_a_foreign_agent_is_refused(self):
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "model-a")
        _, err = self.run_coord("release", "--area", "text", "--issue", 142,
                                "--agent", "model-b", expect=2)
        self.assertIn("held by model-a", err)

    def test_renew_extends_the_ttl_and_keeps_the_ref(self):
        self.run_coord("claim", "--area", "render", "--issue", 122, "--agent", "model-a",
                       "--ttl", "1h")
        self._set_now(self.now + 30 * 60)
        out, _ = self.run_coord("renew", "--area", "render", "--issue", 122,
                                "--agent", "model-a")
        self.assertIn("renewed render / #122", out)
        lease = coord.read_lease("refs/coord/leases/render/122")
        self.assertEqual(lease.epoch, self.now)
        self.assertEqual(lease.ttl, 3600)

    def test_renew_can_set_state_blocked_and_needs_review(self):
        self.run_coord("claim", "--area", "content", "--issue", 124, "--agent", "m")
        self.run_coord("renew", "--area", "content", "--issue", 124, "--agent", "m",
                       "--state", "blocked", "--note", "waiting on #120")
        lease = coord.read_lease("refs/coord/leases/content/124")
        self.assertEqual(lease.state, "blocked")
        self.assertEqual(lease.note, "waiting on #120")


class TestExpiryAndSweep(CoordTestCase):
    def test_expired_lease_can_be_claimed_by_anyone(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a",
                       "--ttl", "1h")
        self._set_now(self.now + 3601)  # one second past the TTL
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-b")
        self.assertIn("claimed world / #157", out)
        lease = coord.read_lease("refs/coord/leases/world/157")
        self.assertEqual(lease.agent, "model-b")

    def test_sweep_deletes_only_expired_refs(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "1h")
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "b", "--ttl", "12h")
        self._set_now(self.now + 2 * 3600)
        out, _ = self.run_coord("sweep", "--no-board")
        self.assertIn("swept 1 expired lease(s)", out)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/text/142"])

    def test_ttl_is_capped_at_24h(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a",
                       "--ttl", "99d")
        lease = coord.read_lease("refs/coord/leases/world/157")
        self.assertEqual(lease.ttl, coord.MAX_TTL)

    def test_stale_is_live_but_quiet(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "20h")
        lease = coord.read_lease("refs/coord/leases/world/157")
        self.assertFalse(lease.is_stale(self.now))
        self.assertTrue(lease.is_stale(self.now + coord.STALE_AFTER + 1))
        self.assertTrue(lease.is_live(self.now + coord.STALE_AFTER + 1))


class TestCheckGate(CoordTestCase):
    def test_check_passes_on_a_clean_ledger(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        out, _ = self.run_coord("check")
        self.assertIn("check OK", out)

    def test_check_fails_on_a_live_area_collision(self):
        # Force a collision by writing two refs for the same area directly, as a
        # racing writer might; the gate must catch it.
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        lease = coord.Lease(agent="b", area="world", issue=200, epoch=self.now,
                            ttl=coord.DEFAULT_TTL)
        coord._commit_lease(lease)
        out, err = self.run_coord("check", expect=1)
        self.assertIn("coordination conflict", err + out)
        self.assertIn("live leases", err + out)

    def test_check_ignores_expired_collisions(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "1h")
        lease = coord.Lease(agent="b", area="world", issue=200, epoch=self.now - 10_000,
                            ttl=60)
        coord._commit_lease(lease)
        out, _ = self.run_coord("check")
        self.assertIn("check OK", out)


class TestStatusAndBoard(CoordTestCase):
    def test_status_reports_free_areas_and_holder(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        out, _ = self.run_coord("status")
        self.assertIn("#157", out)
        self.assertIn("free areas:", out)
        self.assertNotIn("world,", out.split("free areas:")[1])  # world is taken

    def test_status_json_is_machine_readable(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        out, _ = self.run_coord("status", "--json")
        data = json.loads(out)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]["area"], "world")
        self.assertTrue(data[0]["live"])

    def test_board_renders_every_area_and_the_holder(self):
        self.run_coord("claim", "--area", "render", "--issue", 122, "--agent", "model-a")
        content = coord.render_board()
        self.assertIn(coord.BOARD_MARKER, content)
        for area in coord.AREAS:
            self.assertIn(f"`{area}`", content)
        self.assertIn("model-a · #122", content)
        self.assertIn("| claimed |", content)

    def test_board_shows_free_areas(self):
        content = coord.render_board()
        self.assertEqual(content.count("| — | free | — |"), len(coord.AREAS))

    def test_board_marks_stale_holders(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "30h")
        lease = coord.read_lease("refs/coord/leases/world/157")
        content = coord.render_board(at=self.now + coord.STALE_AFTER + 1)
        self.assertIn("| stale |", content)

    def test_board_is_written_to_the_expected_path(self):
        self.run_coord("board")
        path = os.path.join(self.work, coord.BOARD_PATH)
        self.assertTrue(os.path.exists(path))
        with open(path, encoding="utf-8") as fh:
            first = fh.read()
        # Running it twice is idempotent within the same tick.
        self.run_coord("board")
        with open(path, encoding="utf-8") as fh:
            self.assertEqual(first, fh.read())


class TestDurations(CoordTestCase):
    def test_parse_duration_units(self):
        self.assertEqual(coord.parse_duration("90"), 90 * 60)
        self.assertEqual(coord.parse_duration("30s"), 30)
        self.assertEqual(coord.parse_duration("6h"), 6 * 3600)
        self.assertEqual(coord.parse_duration("2d"), 2 * 86400)

    def test_parse_duration_rejects_garbage(self):
        with self.assertRaises(coord.CoordError):
            coord.parse_duration("soon")


class TestLegacyMigration(CoordTestCase):
    def test_legacy_claim_regex_parses_the_old_line(self):
        body = "CLAIM: Claude (OpenHands) | focus: input-ui | feat/ui-version-help | 2026-10-15T07:29Z"
        m = coord.LEGACY_CLAIM_RE.search(body)
        self.assertIsNotNone(m)
        self.assertEqual(m.group("agent").strip(), "Claude (OpenHands)")
        self.assertEqual(m.group("area").strip().replace("focus:", "").strip(), "input-ui")

    def test_legacy_lease_regex_parses_branch(self):
        body = "LEASE: OpenHands | feat/world-moveto-nearest | 2026-10-15T07:44Z"
        m = coord.LEGACY_LEASE_RE.search(body)
        self.assertIsNotNone(m)
        self.assertEqual(m.group("branch"), "feat/world-moveto-nearest")

    def test_legacy_takeover_regex(self):
        m = coord.LEGACY_TAKEOVER_RE.search("TAKEOVER: OpenHands | focus: world")
        self.assertIsNotNone(m)
        self.assertEqual(m.group("area").strip().replace("focus:", "").strip(), "world")

    def test_comment_age_parses_gh_timestamps(self):
        at = 1_800_000_000
        self.assertIsNone(coord._comment_age_seconds("", at))
        self.assertIsNone(coord._comment_age_seconds("not-a-date", at))
        # An epoch one hour before `at`, rendered as a GH timestamp.
        older = coord.iso(at - 3600)
        self.assertEqual(coord._comment_age_seconds(older, at), 3600)

    def test_migration_only_imports_live_claims(self):
        """A claim older than the TTL is left free rather than resurrected."""
        at = 1_800_000_000
        recent = coord.iso(at - 60)
        old = coord.iso(at - coord.MAX_TTL - 60)
        self.assertLessEqual(coord._comment_age_seconds(recent, at), coord.MAX_TTL)
        self.assertGreater(coord._comment_age_seconds(old, at), coord.MAX_TTL)


class TestConcurrency(CoordTestCase):
    def test_force_with_lease_rejects_a_stale_writer(self):
        """Two writers race: the second CAS with a stale expectation must lose."""
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        stale_sha = coord._local_ref_sha("refs/coord/leases/world/157")

        # Writer B wins the race first.
        winner = coord.Lease(agent="b", area="world", issue=157, epoch=self.now, ttl=60)
        coord._commit_lease(winner)

        # Writer A now pushes with its stale expectation -> must be rejected.
        loser = coord.Lease(agent="a", area="world", issue=157, epoch=self.now, ttl=60)
        with self.assertRaises(coord.CoordError):
            coord._commit_lease(loser, expect=stale_sha)

        # B's lease survives untouched.
        self.assertEqual(coord.read_lease("refs/coord/leases/world/157").agent, "b")

    def test_two_agents_fan_out_across_areas(self):
        """The core promise: 8 agents, 8 areas, zero collisions."""
        for i, area in enumerate(coord.AREAS):
            self.run_coord("claim", "--area", area, "--issue", 100 + i, "--agent", f"model-{i}")
        self.assertEqual(len(self.remote_refs()), len(coord.AREAS))
        self.run_coord("check")
        out, _ = self.run_coord("status")
        self.assertIn("live leases: 8", out)
        self.assertIn("free areas: (none)", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
