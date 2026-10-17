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
import unittest
from contextlib import redirect_stdout, redirect_stderr
from datetime import datetime, timezone

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

    def _stub_issues(self, mapping):
        """Stub the GitHub issue lookup so `next` runs with no network."""
        self._orig = coord._open_issues_by_area
        coord._open_issues_by_area = lambda: dict(mapping)
        self.addCleanup(setattr, coord, "_open_issues_by_area", self._orig)

    def _stub_prs(self, rows):
        """Stub the PR lookup; default empty so `next` never reaches the network."""
        self._orig_prs = coord._drainable_prs
        coord._drainable_prs = lambda *a, **k: list(rows)
        self.addCleanup(setattr, coord, "_drainable_prs", self._orig_prs)

    def push_raw_lease_ref(self, ref, lease):
        """Write an arbitrary lease ref out of band, as an outdated client might.

        ``coord`` can no longer *produce* two live leases for one area; this is
        how the tests simulate the one way one can still appear (a pre-fix client
        writing the old per-issue layout).
        """
        empty_tree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904"
        commit = subprocess.run(
            ["git", "commit-tree", empty_tree, "-m", lease.to_json()],
            cwd=self.work, check=True, stdout=subprocess.PIPE, text=True,
            env={**os.environ, "GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@e.com",
                 "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@e.com"},
        ).stdout.strip()
        subprocess.run(["git", "push", "--quiet", "--force", "origin",
                        f"{commit}:{ref}"], cwd=self.work, check=True)


class TestLeaseBasics(CoordTestCase):
    def test_claim_creates_one_ref_per_area(self):
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-a", "--branch", "feat/world-x")
        self.assertIn("claimed world / #157", out)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world"])

    def test_second_agent_cannot_claim_a_held_area(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a")
        _, err = self.run_coord("claim", "--area", "world", "--issue", 200,
                                "--agent", "model-b", expect=2)
        self.assertIn("held by model-a", err)
        # The failed claim must not have created a second ref.
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world"])

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
        lease = coord.read_lease("refs/coord/leases/render")
        self.assertEqual(lease.epoch, self.now)
        self.assertEqual(lease.ttl, 3600)

    def test_renew_can_set_state_blocked_and_needs_review(self):
        self.run_coord("claim", "--area", "content", "--issue", 124, "--agent", "m")
        self.run_coord("renew", "--area", "content", "--issue", 124, "--agent", "m",
                       "--state", "blocked", "--note", "waiting on #120")
        lease = coord.read_lease("refs/coord/leases/content")
        self.assertEqual(lease.state, "blocked")
        self.assertEqual(lease.note, "waiting on #120")


class TestSameNameIdentity(CoordTestCase):
    """Two workspaces running the same model share the `--agent` string.

    A lease records only that string, so `next` must not claim a peer's lease as
    yours, and renew/release must not let you free it. The recorded branch is the
    tiebreaker (issue #365).
    """

    def _on_branch(self, name):
        git(self.work, "checkout", "--quiet", "-b", name)

    def test_next_does_not_claim_a_same_named_peer_lease(self):
        self.run_coord("claim", "--area", "release", "--issue", 350,
                       "--agent", "OpenHands", "--branch", "release/v0.8.0-dominion")
        self._on_branch("platform/size-budget")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "OpenHands")
        # It must not say we hold it, and it must name the peer's branch.
        self.assertNotIn("You hold", out)
        self.assertIn("release/v0.8.0-dominion", out)
        self.assertIn("not this workspace", out)

    def test_next_still_reports_your_own_lease_by_branch(self):
        self.run_coord("claim", "--area", "platform", "--issue", 365,
                       "--agent", "OpenHands", "--branch", "coord/lease-identity")
        self._on_branch("coord/lease-identity")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "OpenHands")
        self.assertIn("You hold platform / #365", out)

    def test_release_of_a_same_named_peer_lease_is_refused(self):
        self.run_coord("claim", "--area", "release", "--issue", 350,
                       "--agent", "OpenHands", "--branch", "release/v0.8.0-dominion")
        self._on_branch("platform/size-budget")
        _, err = self.run_coord("release", "--area", "release", "--issue", 350,
                                "--agent", "OpenHands", expect=2)
        self.assertIn("release/v0.8.0-dominion", err)
        # The peer's lease must still be there.
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/release"])

    def test_renew_of_a_same_named_peer_lease_is_refused(self):
        self.run_coord("claim", "--area", "release", "--issue", 350,
                       "--agent", "OpenHands", "--branch", "release/v0.8.0-dominion")
        before = coord.read_lease("refs/coord/leases/release").epoch
        self._on_branch("platform/size-budget")
        self._set_now(self.now + 60)
        self.run_coord("renew", "--area", "release", "--issue", 350,
                       "--agent", "OpenHands", expect=2)
        self.assertEqual(coord.read_lease("refs/coord/leases/release").epoch, before)

    def test_force_still_frees_a_same_named_peer_lease(self):
        self.run_coord("claim", "--area", "release", "--issue", 350,
                       "--agent", "OpenHands", "--branch", "release/v0.8.0-dominion")
        self._on_branch("platform/size-budget")
        out, _ = self.run_coord("release", "--area", "release", "--issue", 350,
                                "--agent", "OpenHands", "--force")
        self.assertIn("released release", out)
        self.assertEqual(self.remote_refs(), [])

    def test_explicit_branch_releases_your_lease_from_anywhere(self):
        # An agent on another branch names the branch its lease records.
        self.run_coord("claim", "--area", "platform", "--issue", 365,
                       "--agent", "OpenHands", "--branch", "coord/lease-identity")
        self._on_branch("some/other-branch")
        out, _ = self.run_coord("release", "--area", "platform", "--issue", 365,
                                "--agent", "OpenHands", "--branch", "coord/lease-identity")
        self.assertIn("released platform", out)
        self.assertEqual(self.remote_refs(), [])

    def test_branchless_lease_still_matches_by_name(self):
        # Legacy / no --branch: nothing to compare, so name alone still matches
        # (the pre-#365 behaviour, kept so existing leases do not break).
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "OpenHands")
        self._on_branch("any/branch")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "OpenHands")
        self.assertIn("You hold text / #142", out)
        out, _ = self.run_coord("release", "--area", "text", "--issue", 142,
                                "--agent", "OpenHands")
        self.assertIn("released text", out)


class TestExpiryAndSweep(CoordTestCase):
    def test_expired_lease_can_be_claimed_by_anyone(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "model-a",
                       "--ttl", "1h")
        self._set_now(self.now + 3601)  # one second past the TTL
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "model-b")
        self.assertIn("claimed world / #157", out)
        lease = coord.read_lease("refs/coord/leases/world")
        self.assertEqual(lease.agent, "model-b")

    def test_sweep_deletes_only_expired_refs(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "1h")
        self.run_coord("claim", "--area", "text", "--issue", 142, "--agent", "b", "--ttl", "12h")
        self._set_now(self.now + 2 * 3600)
        out, _ = self.run_coord("sweep", "--no-board")
        self.assertIn("swept 1 expired lease(s)", out)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/text"])

    def test_ttl_is_capped_at_24h(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a",
                       "--ttl", "99d")
        lease = coord.read_lease("refs/coord/leases/world")
        self.assertEqual(lease.ttl, coord.MAX_TTL)

    def test_stale_is_live_but_quiet(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a", "--ttl", "20h")
        lease = coord.read_lease("refs/coord/leases/world")
        self.assertFalse(lease.is_stale(self.now))
        self.assertTrue(lease.is_stale(self.now + coord.STALE_AFTER + 1))
        self.assertTrue(lease.is_live(self.now + coord.STALE_AFTER + 1))

    def test_stale_window_is_shorter_than_the_default_ttl(self):
        """Regression: STALE_AFTER was 12 h while DEFAULT_TTL was 6 h, so a lease
        expired before it could ever be flagged stale - the signal was dead code.
        A default lease must be able to *reach* staleness while still live."""
        self.assertLess(coord.STALE_AFTER, coord.DEFAULT_TTL)
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        lease = coord.read_lease("refs/coord/leases/world")
        self.assertTrue(lease.is_stale(self.now + coord.STALE_AFTER + 1))
        self.assertTrue(lease.is_live(self.now + coord.STALE_AFTER + 1))


class TestCheckGate(CoordTestCase):
    def test_check_passes_on_a_clean_ledger(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        out, _ = self.run_coord("check")
        self.assertIn("check OK", out)

    def test_check_fails_on_a_live_legacy_per_issue_ref(self):
        # Git forbids `leases/world` and `leases/world/200` together, so a legacy
        # ref only exists for an area that has no area ref yet.  It means a
        # pre-fix client wrote it; the gate must flag it before the area is used.
        self.push_raw_lease_ref(
            "refs/coord/leases/world/200",
            coord.Lease(agent="b", area="world", issue=200, epoch=self.now,
                        ttl=coord.DEFAULT_TTL))
        out, err = self.run_coord("check", expect=1)
        self.assertIn("coordination conflict", err + out)
        self.assertIn("legacy per-issue ref", err + out)

    def test_check_ignores_expired_legacy_refs(self):
        # An expired legacy-layout ref is not a live conflict; sweep removes it.
        self.push_raw_lease_ref(
            "refs/coord/leases/world/200",
            coord.Lease(agent="b", area="world", issue=200, epoch=self.now - 10_000,
                        ttl=60))
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


class TestNextAction(CoordTestCase):
    """`coord next` must always print an actionable next step - never nothing.

    These tests stub the GitHub calls (`_open_issues_by_area`, `_drainable_prs`)
    so they run with no network, which is also the real fallback path.
    """

    def setUp(self):
        super().setUp()
        # No drainable PR unless a test says otherwise, so rungs 3-5 are reached.
        self._stub_prs([])

    def test_next_returns_your_own_lease_first(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("You hold world / #157", out)
        self.assertIn("renew", out)

    def test_next_prefers_the_critical_path(self):
        self._stub_issues({"metagame": [(249, False)], "world": [(157, False)]})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("metagame / #249", out)
        self.assertIn("critical path", out)

    def test_next_falls_back_to_any_free_area_with_work(self):
        # metagame held, so the critical-path rung is skipped.
        self.run_coord("claim", "--area", "metagame", "--issue", 249, "--agent", "x")
        self._stub_issues({"metagame": [(249, False)], "world": [(157, False)]})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("world / #157", out)

    def test_next_prefers_a_waiting_blocker_over_plain_critical_work(self):
        # metagame (first critical-path area) has only plain work; tactical has a
        # Rule 4 request another lane is blocked on.  Clearing the blocker wins.
        self._stub_issues({"metagame": [(249, False)], "tactical": [(311, True)]})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("tactical / #311", out)
        self.assertNotIn("#249", out)

    def test_next_seeds_from_the_roadmap_when_the_backlog_is_empty(self):
        self._stub_issues({})  # no open issues at all
        out, _ = self.run_coord("next", "--agent", "a")
        # Not a claim: it must tell the agent how to create work, not stall.
        self.assertIn("file an issue", out)
        self.assertIn("focus:", out)

    def test_next_offers_review_when_every_area_is_held(self):
        for i, area in enumerate(coord.AREAS):
            self.run_coord("claim", "--area", area, "--issue", 100 + i, "--agent", "x")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("review and merge an open PR", out)

    def test_next_offers_takeover_of_a_stale_holder(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "x")
        for area in coord.AREAS:
            if area != "world":
                self.run_coord("claim", "--area", area, "--issue", 200,
                               "--agent", "x")
        self._set_now(self.now + coord.STALE_AFTER + 1)
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("stale", out)
        self.assertIn("--takeover", out)

    def test_next_never_prints_nothing(self):
        """The whole point: whatever the state, there is a line of output."""
        for mapping in ({}, {"world": [(1, False)]}, {"metagame": [(249, False)]}):
            self._stub_issues(mapping)
            out, _ = self.run_coord("next", "--agent", "a")
            self.assertTrue(out.strip(), f"empty output for {mapping}")

    def test_next_drains_a_green_pr_before_claiming_plain_work(self):
        # A non-critical area has claimable work, but a green PR has waited: the
        # finished work one step from landing is offered first (ADR-0035).
        # `render` is off the critical path (issue #425), so rung 2 cannot fire.
        self._stub_issues({"render": [(157, False)]})
        self._stub_prs([{"number": 336, "title": "test(i18n): guard ...",
                         "headRefName": "test/text-x", "_age": 3600}])
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("Merge queue: PR #336", out)
        self.assertIn("gh pr merge 336", out)
        self.assertNotIn("claim render / #157", out)

    def test_next_does_not_drain_before_the_critical_path(self):
        # The critical path still wins: a free critical-path issue outranks the
        # merge queue, so the release is never starved by draining.
        self._stub_issues({"metagame": [(249, False)]})
        self._stub_prs([{"number": 336, "title": "x", "_age": 3600}])
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("metagame / #249", out)
        self.assertNotIn("Merge queue", out)

    def test_next_drains_before_a_free_areas_plain_work(self):
        # No critical-path work; a free area has work and a PR has waited.
        # Hold every critical-path area (issue #425 order) and offer work only in
        # an off-path area, so rung 4 (plain work) competes with the merge queue.
        self.run_coord("claim", "--area", "tactical", "--issue", 311, "--agent", "x")
        self.run_coord("claim", "--area", "metagame", "--issue", 249, "--agent", "x")
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "x")
        self._stub_issues({"render": [(157, False)]})
        self._stub_prs([{"number": 342, "title": "feat: vehicles", "_age": 7200}])
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("Merge queue: PR #342", out)

    def test_next_reaches_claims_when_the_queue_is_empty(self):
        self._stub_issues({"world": [(157, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("world / #157", out)
        self.assertNotIn("Merge queue", out)

    def test_next_keeps_your_own_lease_ahead_of_the_merge_queue(self):
        # Rung 1 is absolute: you finish your own lease before adopting a PR.
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        self._stub_issues({})
        self._stub_prs([{"number": 336, "title": "x", "_age": 3600}])
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("You hold world / #157", out)
        self.assertNotIn("Merge queue", out)


class TestDrainablePrs(CoordTestCase):
    """`_drainable_prs` filters to green, non-draft, waited PRs and orders FIFO."""

    def _stub_gh_prs(self, rows):
        def fake_gh(*args, **kwargs):
            if args[:2] == ("pr", "list"):
                return subprocess.CompletedProcess(
                    args, 0, stdout=json.dumps(rows), stderr="")
            return subprocess.CompletedProcess(args, 0, stdout="", stderr="")

        orig = coord.gh
        coord.gh = fake_gh
        self.addCleanup(setattr, coord, "gh", orig)

    def _pr(self, number, *, draft=False, state="CLEAN", age=3600, title="t"):
        created = self.now - age
        stamp = datetime.fromtimestamp(created, timezone.utc).strftime(
            "%Y-%m-%dT%H:%M:%SZ")
        return {"number": number, "title": title, "isDraft": draft,
                "mergeStateStatus": state, "createdAt": stamp,
                "headRefName": f"b/{number}"}

    def test_draft_prs_are_not_drainable(self):
        self._stub_gh_prs([self._pr(1, draft=True)])
        self.assertEqual(coord._drainable_prs(), [])

    def test_non_clean_prs_are_not_drainable(self):
        for state in ("UNSTABLE", "DIRTY", "BLOCKED", "BEHIND"):
            self._stub_gh_prs([self._pr(1, state=state)])
            self.assertEqual(coord._drainable_prs(), [], state)

    def test_young_prs_are_not_drainable_yet(self):
        self._stub_gh_prs([self._pr(1, age=coord.MERGE_DRAIN_MIN_AGE - 1)])
        self.assertEqual(coord._drainable_prs(), [])

    def test_waited_clean_pr_is_drainable(self):
        self._stub_gh_prs([self._pr(1, age=coord.MERGE_DRAIN_MIN_AGE + 1)])
        got = coord._drainable_prs()
        self.assertEqual([p["number"] for p in got], [1])

    def test_oldest_pr_is_offered_first(self):
        self._stub_gh_prs([self._pr(1, age=3600), self._pr(2, age=9000),
                           self._pr(3, age=6000)])
        got = coord._drainable_prs()
        self.assertEqual([p["number"] for p in got], [2, 3, 1])

    def test_iso_epoch_parses_z_and_rejects_junk(self):
        self.assertEqual(coord._iso_epoch("1970-01-01T00:01:00Z"), 60)
        self.assertIsNone(coord._iso_epoch(""))
        self.assertIsNone(coord._iso_epoch("not-a-date"))

    def test_gh_failure_yields_no_prs(self):
        def failing(*a, **k):
            return subprocess.CompletedProcess(a, 1, stdout="", stderr="boom")

        orig = coord.gh
        coord.gh = failing
        self.addCleanup(setattr, coord, "gh", orig)
        self.assertEqual(coord._drainable_prs(), [])


class TestClaimableIssueRouting(CoordTestCase):
    """`next` must only offer *claimable* work, and clear blockers first.

    Routing an idle agent to a blocked/question/deferred issue, or past a Rule 4
    request another lane is waiting on, is a wasted cycle (FOCUS_AREAS §4).
    """

    def _stub_gh(self, rows):
        def fake_gh(*args, **kwargs):
            if args[:2] == ("issue", "list"):
                return subprocess.CompletedProcess(
                    args, 0, stdout=json.dumps(rows), stderr="")
            return subprocess.CompletedProcess(args, 0, stdout="", stderr="")

        orig = coord.gh
        coord.gh = fake_gh
        self.addCleanup(setattr, coord, "gh", orig)

    def test_unclaimable_issues_are_not_offered(self):
        self._stub_gh([
            {"number": 1, "createdAt": "2026-01-01T00:00:00Z",
             "labels": [{"name": "focus: tactical"}, {"name": "status: blocked"}]},
            {"number": 2, "createdAt": "2026-01-02T00:00:00Z",
             "labels": [{"name": "focus: tactical"}, {"name": "question"}]},
            {"number": 3, "createdAt": "2026-01-03T00:00:00Z",
             "labels": [{"name": "focus: tactical"}, {"name": "deferred: v2"}]},
        ])
        self.assertEqual(coord._open_issues_by_area(), {})

    def test_rule4_request_sorts_ahead_of_newer_plain_work(self):
        # #10 is newer, but #9 is a Rule 4 request another lane is waiting on, so
        # clearing it unblocks more than starting #10 would.
        self._stub_gh([
            {"number": 10, "createdAt": "2026-01-10T00:00:00Z",
             "labels": [{"name": "focus: tactical"}]},
            {"number": 9, "createdAt": "2026-01-09T00:00:00Z",
             "labels": [{"name": "focus: tactical"},
                        {"name": "status: needs-review"}]},
        ])
        self.assertEqual(coord._open_issues_by_area()["tactical"],
                         [(9, True), (10, False)])

    def test_newest_first_within_equal_rank(self):
        self._stub_gh([
            {"number": 10, "createdAt": "2026-01-10T00:00:00Z",
             "labels": [{"name": "focus: world"}]},
            {"number": 11, "createdAt": "2026-01-11T00:00:00Z",
             "labels": [{"name": "focus: world"}]},
        ])
        self.assertEqual(coord._open_issues_by_area()["world"],
                         [(11, False), (10, False)])

    def test_gh_failure_yields_an_empty_map(self):
        def failing(*a, **k):
            return subprocess.CompletedProcess(a, 1, stdout="", stderr="boom")

        orig = coord.gh
        coord.gh = failing
        self.addCleanup(setattr, coord, "gh", orig)
        self.assertEqual(coord._open_issues_by_area(), {})


class TestStatusLabelReconcile(CoordTestCase):
    """The `status:` labels are a mirror; the reconciler clears only lease-set ones."""

    def _stub_gh(self, rows):
        calls = []

        def fake_gh(*args, **kwargs):
            calls.append(args)
            if args[:2] == ("issue", "list"):
                return subprocess.CompletedProcess(
                    args, 0, stdout=json.dumps(rows), stderr="")
            return subprocess.CompletedProcess(args, 0, stdout="", stderr="")

        orig = coord.gh
        coord.gh = fake_gh
        self.addCleanup(setattr, coord, "gh", orig)
        return calls

    def test_reconcile_clears_claimed_on_an_open_issue_with_no_lease(self):
        # A released lease leaves `status: claimed` behind; it must be cleared.
        calls = self._stub_gh([
            {"number": 219, "state": "OPEN",
             "labels": [{"name": "status: claimed"}]},
            {"number": 249, "state": "OPEN",
             "labels": [{"name": "status: claimed"}]},
        ])
        coord._reconcile_status_labels({249})  # #249 has a live lease
        edited = [c for c in calls if c[:2] == ("issue", "edit")]
        self.assertEqual(len(edited), 1)
        self.assertEqual(edited[0][2], "219")
        self.assertIn("--remove-label", edited[0])

    def test_reconcile_leaves_a_human_question_blocked_label_alone(self):
        # `status: blocked` / `status: needs-review` have no lease (they belong to
        # the human-question and Rule 4 conventions), so they must not be cleared.
        calls = self._stub_gh([
            {"number": 216, "state": "OPEN",
             "labels": [{"name": "question"}, {"name": "status: blocked"}]},
            {"number": 294, "state": "OPEN",
             "labels": [{"name": "status: needs-review"}]},
        ])
        coord._reconcile_status_labels(set())
        edited = [c for c in calls if c[:2] == ("issue", "edit")]
        self.assertEqual(edited, [])

    def test_reconcile_asks_only_for_open_issues(self):
        # Closed issues are historical; the sweep must not mass-edit them.
        calls = self._stub_gh([])
        coord._reconcile_status_labels(set())
        listed = [c for c in calls if c[:2] == ("issue", "list")]
        self.assertEqual(len(listed), 1)
        self.assertIn("--state", listed[0])
        self.assertEqual(listed[0][listed[0].index("--state") + 1], "open")

    def test_reconcile_never_raises_when_gh_is_unavailable(self):
        def failing_gh(*args, **kwargs):
            return subprocess.CompletedProcess(args, 1, stdout="", stderr="boom")

        orig = coord.gh
        coord.gh = failing_gh
        self.addCleanup(setattr, coord, "gh", orig)
        coord._reconcile_status_labels(set())  # must not raise


class TestResolvedBlockReconcile(CoordTestCase):
    """A `blocked-by:` block lifts itself once its blocker(s) close (ADR-0030)."""

    def _stateful_gh(self, issues):
        """A fake gh over ``issues`` that mutates, so a test sees the real result.

        `issue list` returns the current rows (each carrying number/body/labels/
        createdAt, which every reader accepts); `issue edit --remove-label` drops
        the label, exactly as the sweep would leave it.
        """
        calls = []

        def fake_gh(*args, **kwargs):
            calls.append(args)
            if args[:2] == ("issue", "list"):
                rows = [{"number": i["number"], "body": i.get("body", ""),
                         "labels": [{"name": n} for n in i["labels"]],
                         "createdAt": "2026-01-01T00:00:00Z"} for i in issues]
                return subprocess.CompletedProcess(
                    args, 0, stdout=json.dumps(rows), stderr="")
            if args[:2] == ("issue", "edit"):
                number = int(args[2])
                label = args[args.index("--remove-label") + 1]
                for i in issues:
                    if i["number"] == number and label in i["labels"]:
                        i["labels"].remove(label)
                return subprocess.CompletedProcess(args, 0, stdout="", stderr="")
            return subprocess.CompletedProcess(args, 0, stdout="", stderr="")

        orig = coord.gh
        coord.gh = fake_gh
        self.addCleanup(setattr, coord, "gh", orig)
        return calls

    def test_blocked_by_parses_and_ignores_prose(self):
        self.assertEqual(coord.blocked_by_refs("blocked-by: #311"), {311})
        self.assertEqual(coord.blocked_by_refs("Blocked-By: #311, #312"), {311, 312})
        # Prose that merely mentions an issue is not a block.
        self.assertEqual(coord.blocked_by_refs("see #311 and #312"), set())
        self.assertEqual(coord.blocked_by_refs(""), set())

    def test_clears_when_all_blockers_closed(self):
        issues = [{"number": 312, "body": "blocked-by: #311",
                   "labels": ["focus: metagame", "status: blocked"]}]
        self._stateful_gh(issues)
        coord._clear_resolved_blocks(open_numbers=set())  # nothing is open
        self.assertNotIn("status: blocked", issues[0]["labels"])

    def test_keeps_block_while_a_blocker_is_open(self):
        issues = [{"number": 312, "body": "blocked-by: #311",
                   "labels": ["focus: metagame", "status: blocked"]}]
        self._stateful_gh(issues)
        coord._clear_resolved_blocks(open_numbers={311})
        self.assertIn("status: blocked", issues[0]["labels"])

    def test_keeps_block_while_any_of_several_blockers_is_open(self):
        issues = [{"number": 312, "body": "blocked-by: #311, #313",
                   "labels": ["focus: metagame", "status: blocked"]}]
        self._stateful_gh(issues)
        coord._clear_resolved_blocks(open_numbers={313})  # #313 still open
        self.assertIn("status: blocked", issues[0]["labels"])

    def test_leaves_a_human_question_alone(self):
        # A §5.2 question carries `status: blocked` with no `blocked-by:` line.
        issues = [{"number": 216, "body": "Should we add a shield?",
                   "labels": ["question", "status: blocked"]}]
        self._stateful_gh(issues)
        coord._clear_resolved_blocks(open_numbers=set())
        self.assertIn("status: blocked", issues[0]["labels"])

    def test_never_clears_when_github_is_unreadable(self):
        # `None` is "unknown", not "everything closed": a live block must survive.
        issues = [{"number": 312, "body": "blocked-by: #311",
                   "labels": ["focus: metagame", "status: blocked"]}]
        self._stateful_gh(issues)
        coord._clear_resolved_blocks(open_numbers=None)
        self.assertIn("status: blocked", issues[0]["labels"])

    def test_never_raises_when_gh_is_unavailable(self):
        def failing_gh(*args, **kwargs):
            return subprocess.CompletedProcess(args, 1, stdout="", stderr="boom")

        orig = coord.gh
        coord.gh = failing_gh
        self.addCleanup(setattr, coord, "gh", orig)
        coord._clear_resolved_blocks(open_numbers=set())  # must not raise

    def test_resolved_block_becomes_claimable_again(self):
        # Acceptance (#326): after the blocker closes, `next` offers the issue.
        issues = [{"number": 312, "body": "blocked-by: #311",
                   "labels": ["focus: metagame", "status: blocked"]}]
        self._stateful_gh(issues)
        self.assertEqual(coord._open_issues_by_area(), {})  # blocked: not offered
        coord._clear_resolved_blocks(open_numbers=set())
        self.assertEqual(coord._open_issues_by_area()["metagame"], [(312, False)])


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
    def test_area_lock_cas_blocks_a_second_issue_in_the_same_area(self):
        """Regression: two agents claim *different* issues in one area at once.

        Both pass their pre-checks believing the area is free.  Because the CAS
        target is the *area* ref, only one can win.  (Keying the ref by issue let
        both win - the bug this guards.)
        """
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        with self.assertRaises(coord.CoordError):
            coord._commit_leases(
                [coord.Lease(agent="b", area="world", issue=200, epoch=self.now, ttl=60)],
                expects={"refs/coord/leases/world": ""})  # "must not exist"
        self.assertEqual(coord.read_lease("refs/coord/leases/world").agent, "a")

    def test_force_with_lease_rejects_a_stale_writer(self):
        """Two writers race: the second CAS with a stale expectation must lose."""
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        stale_sha = coord._local_ref_sha("refs/coord/leases/world")

        # Writer B wins the race first.
        winner = coord.Lease(agent="b", area="world", issue=157, epoch=self.now, ttl=60)
        coord._commit_leases([winner], expects={"refs/coord/leases/world": None})

        # Writer A now pushes with its stale expectation -> must be rejected.
        loser = coord.Lease(agent="a", area="world", issue=157, epoch=self.now, ttl=60)
        with self.assertRaises(coord.CoordError):
            coord._commit_leases([loser], expects={"refs/coord/leases/world": stale_sha})

        # B's lease survives untouched.
        self.assertEqual(coord.read_lease("refs/coord/leases/world").agent, "b")

    def test_multi_area_claim_is_all_or_nothing(self):
        """A multi-area claim either takes every area or none of them."""
        self.run_coord("claim", "--area", "world", "--issue", 1, "--agent", "a")
        with self.assertRaises(coord.CoordError):
            coord._commit_leases(
                [coord.Lease(agent="b", area="text", issue=2, epoch=self.now, ttl=60),
                 coord.Lease(agent="b", area="world", issue=2, epoch=self.now, ttl=60)],
                expects={"refs/coord/leases/text": "", "refs/coord/leases/world": ""})
        # The uncontested area must not have been taken by the failed batch.
        self.assertNotIn("refs/coord/leases/text", self.remote_refs())

    def test_legacy_per_issue_ref_is_swept_and_area_unblocked(self):
        """A pre-fix client's ref must be clearable so the area can be claimed."""
        self.push_raw_lease_ref(
            "refs/coord/leases/world/200",
            coord.Lease(agent="old", area="world", issue=200, epoch=self.now - 10_000,
                        ttl=60))
        # The old-layout ref blocks the area ref (git file/dir conflict), so a
        # plain claim fails with guidance rather than a raw git error...
        _, err = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "a", expect=2)
        self.assertIn("legacy per-issue ref", err)
        # ...sweep clears it (it is expired, so it goes with the expired refs)...
        self.run_coord("sweep", "--no-board")
        self.assertEqual(self.remote_refs(), [])
        # ...and then the area is claimable.
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world"])

    def test_takeover_clears_a_live_legacy_ref_and_claims(self):
        """--takeover must not dead-end on a pre-fix client's live ref."""
        self.push_raw_lease_ref(
            "refs/coord/leases/world/200",
            coord.Lease(agent="old", area="world", issue=200, epoch=self.now,
                        ttl=coord.DEFAULT_TTL))
        out, _ = self.run_coord("claim", "--area", "world", "--issue", 157,
                                "--agent", "a", "--takeover")
        self.assertIn("took over world / #157", out)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world"])
        self.assertEqual(coord.read_lease("refs/coord/leases/world").agent, "a")

    def test_two_agents_fan_out_across_areas(self):
        """The core promise: one agent per area, zero collisions."""
        for i, area in enumerate(coord.AREAS):
            self.run_coord("claim", "--area", area, "--issue", 100 + i, "--agent", f"model-{i}")
        self.assertEqual(len(self.remote_refs()), len(coord.AREAS))
        self.run_coord("check")
        out, _ = self.run_coord("status")
        self.assertIn(f"live leases: {len(coord.AREAS)}", out)
        self.assertIn("free areas: (none)", out)


class TestPushAuthDiagnosis(CoordTestCase):
    """A credential failure must not be reported as a lost lease race."""

    def test_push_hint_recognizes_credential_failures(self):
        for text in ("fatal: Authentication failed for 'https://github.com/x'",
                     "fatal: could not read Username for 'https://github.com'",
                     "remote: Invalid username or password.",
                     "Permission denied (publickey)."):
            self.assertIsNotNone(coord._push_hint(text), text)

    def test_push_hint_ignores_a_plain_non_fast_forward(self):
        detail = ("! [rejected] refs/coord/leases/world -> refs/coord/leases/world "
                  "(stale info)")
        self.assertIsNone(coord._push_hint(detail))

    def test_auth_failure_raises_auth_error_not_a_race(self):
        orig = coord._commit_leases

        def boom(*a, **k):
            raise coord.CoordAuthError("authentication failed")

        coord._commit_leases = boom
        self.addCleanup(setattr, coord, "_commit_leases", orig)
        args = coord.build_parser().parse_args(
            ["claim", "--area", "world", "--issue", "1", "--agent", "a"])
        with self.assertRaises(coord.CoordAuthError):
            coord.cmd_claim(args)

    def test_auth_error_is_reported_verbatim_by_claim(self):
        orig = coord._commit_leases

        def boom(*a, **k):
            raise coord.CoordAuthError("could not publish: fix your credentials")

        coord._commit_leases = boom
        self.addCleanup(setattr, coord, "_commit_leases", orig)
        _, err = self.run_coord("claim", "--area", "world", "--issue", 1,
                                "--agent", "a", expect=2)
        self.assertIn("fix your credentials", err)
        self.assertNotIn("ledger moved", err)
        self.assertNotIn("lost the race", err)


class TestBoardPublish(CoordTestCase):
    """The sweep must publish the board or fail loudly - never green and stale."""

    BOARD = "docs/COORDINATION_BOARD.md"

    def _write_board(self, text):
        os.makedirs(os.path.dirname(self.BOARD), exist_ok=True)
        with open(self.BOARD, "w", encoding="utf-8") as fh:
            fh.write(text)

    def _remote_show(self, path):
        return subprocess.run(
            ["git", "--git-dir", self.remote, "show", f"main:{path}"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def _remote_main(self):
        return subprocess.run(
            ["git", "--git-dir", self.remote, "rev-parse", "main"],
            stdout=subprocess.PIPE, text=True).stdout.strip()

    def _advance_remote_main(self):
        """A commit on main this checkout has not seen - a concurrent merge."""
        other = os.path.join(self.tmp, "other")
        git(self.tmp, "clone", "--quiet", "--branch", "main", self.remote, other)
        git(other, "config", "user.email", "o@example.com")
        git(other, "config", "user.name", "Other")
        with open(os.path.join(other, "MERGED.txt"), "w") as fh:
            fh.write("a PR merged while the sweep ran\n")
        git(other, "add", "MERGED.txt")
        git(other, "commit", "--quiet", "-m", "feat: a concurrent merge")
        git(other, "push", "--quiet", "origin", "main")

    def test_commit_board_publishes_to_main(self):
        self._write_board("board v1\n")
        coord._commit_board(self.BOARD)
        shown = self._remote_show(self.BOARD)
        self.assertEqual(shown.returncode, 0, shown.stderr)
        self.assertEqual(shown.stdout, "board v1\n")

    def test_commit_board_uses_the_documented_message(self):
        self._write_board("board v1\n")
        coord._commit_board(self.BOARD)
        proc = subprocess.run(
            ["git", "--git-dir", self.remote, "log", "-1", "--format=%s", "main"],
            stdout=subprocess.PIPE, text=True)
        self.assertEqual(proc.stdout.strip(), coord.BOARD_COMMIT_MSG)

    def test_unchanged_board_makes_no_commit(self):
        self._write_board("board v1\n")
        coord._commit_board(self.BOARD)
        before = self._remote_main()
        # The board on main already matches the file, so a second sweep is a no-op.
        coord._commit_board(self.BOARD)
        self.assertEqual(before, self._remote_main())

    def test_commit_board_retries_past_a_concurrent_merge(self):
        # main advances after this checkout was taken, so a plain push is a
        # non-fast-forward. The board must still land, on top of the merge - and
        # the concurrent merge must survive (no force).
        self._advance_remote_main()
        self._write_board("board v2\n")
        coord._commit_board(self.BOARD)
        shown = self._remote_show(self.BOARD)
        self.assertEqual(shown.returncode, 0, shown.stderr)
        self.assertEqual(shown.stdout, "board v2\n")
        merged = self._remote_show("MERGED.txt")
        self.assertEqual(merged.returncode, 0, merged.stderr)

    def test_rejected_push_raises_instead_of_reporting_success(self):
        # Every push is rejected. The old code swallowed this and printed
        # "board committed and pushed" while main stayed stale (issue #381).
        hook = os.path.join(self.remote, "hooks", "pre-receive")
        with open(hook, "w") as fh:
            fh.write("#!/bin/sh\nexit 1\n")
        os.chmod(hook, 0o755)
        self._write_board("board v1\n")
        with self.assertRaises(coord.CoordError) as ctx:
            coord._commit_board(self.BOARD)
        self.assertIn("could not publish", str(ctx.exception))
        self.assertNotEqual(self._remote_show(self.BOARD).returncode, 0)

    def test_commit_board_supplies_identity_when_environment_has_none(self):
        # A fresh CI checkout has no user.email/user.name. The commit must not be
        # silently skipped, which is how the board stopped landing (issue #381).
        orig_global = os.environ.pop("GIT_CONFIG_GLOBAL", None)
        orig_system = os.environ.pop("GIT_CONFIG_SYSTEM", None)

        def restore():
            for key, value in (("GIT_CONFIG_GLOBAL", orig_global),
                               ("GIT_CONFIG_SYSTEM", orig_system)):
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

        self.addCleanup(restore)
        os.environ["GIT_CONFIG_GLOBAL"] = os.devnull
        os.environ["GIT_CONFIG_SYSTEM"] = os.devnull
        git(self.work, "config", "--unset", "user.email")
        git(self.work, "config", "--unset", "user.name")
        self.assertIsNone(coord.git("config", "user.email", check=False).stdout.strip() or None)
        self._write_board("board v3\n")
        coord._commit_board(self.BOARD)
        shown = self._remote_show(self.BOARD)
        self.assertEqual(shown.returncode, 0, shown.stderr)
        self.assertEqual(shown.stdout, "board v3\n")


class TestReleaseHonesty(CoordTestCase):
    """A release that does not land must not report success (issue #392)."""

    def _reject_all_pushes(self):
        hook = os.path.join(self.remote, "hooks", "pre-receive")
        with open(hook, "w") as fh:
            fh.write("#!/bin/sh\nexit 1\n")
        os.chmod(hook, 0o755)

    def test_release_reports_success_only_when_the_ref_is_gone(self):
        self.run_coord("claim", "--area", "platform", "--issue", 387,
                       "--agent", "a", "--branch", "b")
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/platform"])
        self.run_coord("release", "--area", "platform", "--issue", 387,
                       "--agent", "a", "--branch", "b")
        self.assertEqual(self.remote_refs(), [])

    def test_release_raises_when_the_delete_push_is_rejected(self):
        self.run_coord("claim", "--area", "platform", "--issue", 387,
                       "--agent", "a", "--branch", "b")
        self._reject_all_pushes()
        _, err = self.run_coord("release", "--area", "platform", "--issue", 387,
                                "--agent", "a", "--branch", "b", expect=2)
        self.assertIn("could not delete", err)
        self.assertNotIn("released platform", err)
        # The lease is still held on the remote - the area is NOT free.
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/platform"])

    def test_release_reports_a_credential_failure_as_auth_not_a_race(self):
        self.run_coord("claim", "--area", "platform", "--issue", 387,
                       "--agent", "a", "--branch", "b")
        orig = coord._delete_lease_ref

        def boom(ref, cwd=None):
            raise coord.CoordAuthError("could not delete " + ref)

        coord._delete_lease_ref = boom
        self.addCleanup(setattr, coord, "_delete_lease_ref", orig)
        _, err = self.run_coord("release", "--area", "platform", "--issue", 387,
                                "--agent", "a", "--branch", "b", expect=2)
        self.assertIn("could not delete", err)
        self.assertNotIn("lost the race", err)

    def test_delete_refs_for_area_raises_if_the_remote_refuses(self):
        self.run_coord("claim", "--area", "world", "--issue", 1,
                       "--agent", "a", "--branch", "b")
        self._reject_all_pushes()
        with self.assertRaises(coord.CoordError):
            coord.delete_refs_for_area("world")
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/world"])

    def test_sweep_aborts_when_an_expired_ref_cannot_be_deleted(self):
        # An expired lease is swept by deleting its ref; if that delete is
        # refused the sweep must fail, not print "swept 1" while the ref lives on.
        self.run_coord("claim", "--area", "platform", "--issue", 387,
                       "--agent", "a", "--branch", "b")
        self._set_now(self.now + coord.DEFAULT_TTL + 1)
        self._reject_all_pushes()
        _, err = self.run_coord("sweep", "--no-board", expect=2)
        self.assertIn("could not delete", err)
        self.assertNotIn("swept 1 expired", err)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/platform"])


class TestLedgerReadHonesty(CoordTestCase):
    """A read that could not reach the ledger must not look like an empty one.

    `fetch_ledger` used to ignore every failure, so a fetch that did not land
    left the caller reading an empty namespace: `check` printed "no collisions"
    without having seen a single lease, and `sweep` deleted nothing while
    stripping every `status: claimed` label. The CI gate compounded it by
    fetching with `|| true` and then checking `--offline`.
    """

    def _break_remote(self):
        """Point origin at a path that is not a repository, so fetch fails."""
        git(self.work, "remote", "set-url", "origin",
            os.path.join(self.tmp, "gone.git"))

    def test_fetch_ledger_default_tolerates_a_failed_fetch(self):
        # Reads that only display stay best-effort: no raise, no output change.
        self._break_remote()
        coord.fetch_ledger()

    def test_fetch_ledger_required_raises_when_the_fetch_fails(self):
        self._break_remote()
        with self.assertRaises(coord.CoordError):
            coord.fetch_ledger(required=True)

    def test_check_fails_loudly_when_the_ledger_cannot_be_read(self):
        self.run_coord("claim", "--area", "platform", "--issue", 1,
                       "--agent", "a", "--branch", "b")
        self._break_remote()
        out, err = self.run_coord("check", expect=2)
        self.assertIn("could not read the lease ledger", err)
        # The vacuous pass is exactly what must not happen.
        self.assertNotIn("coordination check OK", out)

    def test_sweep_does_not_delete_on_a_failed_fetch(self):
        self.run_coord("claim", "--area", "platform", "--issue", 1,
                       "--agent", "a", "--branch", "b")
        self._set_now(self.now + coord.DEFAULT_TTL + 1)  # now expired
        self._break_remote()
        _, err = self.run_coord("sweep", "--no-board", expect=2)
        self.assertIn("could not read the lease ledger", err)
        self.assertNotIn("swept", err)
        # The expired ref is untouched, not silently "swept".
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/platform"])

    def test_release_does_not_proceed_on_a_failed_fetch(self):
        self.run_coord("claim", "--area", "platform", "--issue", 1,
                       "--agent", "a", "--branch", "b")
        self._break_remote()
        _, err = self.run_coord("release", "--area", "platform", "--issue", 1,
                                "--agent", "a", "--branch", "b", expect=2)
        self.assertIn("could not read the lease ledger", err)
        self.assertEqual(self.remote_refs(), ["refs/coord/leases/platform"])


class TestNotes(CoordTestCase):
    """Notes are cross-cycle handoff: they outlive the lease that wrote them."""

    def test_note_appends_and_reads_back(self):
        self.run_coord("note", "--area", "platform", "--agent", "a", "--text", "hello")
        self.run_coord("note", "--area", "platform", "--agent", "b", "--text", "world")
        notes = coord.read_notes("platform")
        self.assertEqual([n.text for n in notes], ["hello", "world"])
        self.assertEqual([n.agent for n in notes], ["a", "b"])

    def test_notes_survive_the_lease_being_released(self):
        self.run_coord("claim", "--area", "platform", "--issue", 347, "--agent", "a")
        self.run_coord("note", "--area", "platform", "--agent", "a",
                       "--text", "auth is broken")
        self.run_coord("release", "--area", "platform", "--issue", 347, "--agent", "a")
        self.assertNotIn("refs/coord/leases/platform", self.remote_refs())
        self.assertIn("refs/coord/notes/platform", self.remote_refs())
        self.assertEqual([n.text for n in coord.read_notes("platform")],
                         ["auth is broken"])

    def test_notes_are_pushed_to_the_remote(self):
        self.run_coord("note", "--area", "text", "--agent", "a", "--text", "x")
        self.assertIn("refs/coord/notes/text", self.remote_refs())

    def test_notes_are_capped_to_the_most_recent(self):
        for i in range(coord.MAX_NOTES + 5):
            self.run_coord("note", "--area", "world", "--agent", "a", "--text", f"n{i}")
        notes = coord.read_notes("world")
        self.assertEqual(len(notes), coord.MAX_NOTES)
        self.assertEqual(notes[0].text, "n5")
        self.assertEqual(notes[-1].text, f"n{coord.MAX_NOTES + 4}")

    def test_notes_command_lists_every_area(self):
        self.run_coord("note", "--area", "world", "--agent", "a", "--text", "w-note")
        self.run_coord("note", "--area", "render", "--agent", "b", "--text", "r-note")
        out, _ = self.run_coord("notes", "--offline")
        self.assertIn("[world]", out)
        self.assertIn("w-note", out)
        self.assertIn("[render]", out)
        self.assertIn("r-note", out)

    def test_notes_command_reports_when_empty(self):
        out, _ = self.run_coord("notes", "--offline")
        self.assertIn("no notes", out)

    def test_notes_json_is_machine_readable(self):
        self.run_coord("note", "--area", "text", "--agent", "a", "--text", "j")
        out, _ = self.run_coord("notes", "--area", "text", "--json", "--offline")
        payload = json.loads(out)
        self.assertEqual(payload["text"][0]["text"], "j")

    def test_board_shows_handoff_notes(self):
        self.run_coord("note", "--area", "platform", "--agent", "a",
                       "--text", "read me first")
        content = coord.render_board()
        self.assertIn("## Handoff notes", content)
        self.assertIn("read me first", content)

    def test_board_omits_the_notes_section_when_there_are_none(self):
        self.assertNotIn("## Handoff notes", coord.render_board())

    def test_a_note_does_not_create_or_hold_a_lease(self):
        self.run_coord("note", "--area", "metagame", "--agent", "a", "--text", "fyi")
        self.assertNotIn("refs/coord/leases/metagame", self.remote_refs())
        self.run_coord("check")

    def test_concurrent_appends_do_not_lose_a_note(self):
        """Two agents appending to one area both keep their note."""
        first = coord.Note(agent="a", area="text", text="one", epoch=self.now)
        coord._write_notes("text", [first])
        # A second writer that saw the empty log races: its push must not
        # clobber, so append_note re-reads and merges under the compare-and-swap.
        coord.append_note("text", "b", "two")
        texts = [n.text for n in coord.read_notes("text")]
        self.assertIn("one", texts)
        self.assertIn("two", texts)

    def _second_clone(self):
        """A second clone of the same ledger, as a peer agent would have."""
        work2 = os.path.join(self.tmp, "work2")
        git(self.tmp, "clone", "--quiet", self.remote, work2)
        git(work2, "config", "user.email", "t2@example.com")
        git(work2, "config", "user.name", "Test2")
        return work2

    def test_a_stale_append_is_refused_not_clobbered(self):
        """A peer whose snapshot predates our push must not overwrite it.

        A plain `--force` never conflicts, so it silently drops the note the
        other clone just pushed. The per-ref compare-and-swap refuses instead.
        """
        work2 = self._second_clone()
        # The peer reads the empty log and sees the remote ref is absent.
        coord.fetch_notes(cwd=work2)
        self.assertEqual(coord.read_notes("text", cwd=work2), [])
        expect = coord._local_ref_sha(coord.notes_ref_for("text"), cwd=work2)
        self.assertIsNone(expect)
        # We land a note in the meantime.
        self.run_coord("note", "--area", "text", "--agent", "a", "--text", "one")
        # The peer's write, built on the now-stale snapshot, must be refused
        # rather than overwrite our note.
        stale = coord.Note(agent="b", area="text", text="two", epoch=self.now)
        with self.assertRaises(coord.CoordError):
            coord._write_notes("text", [stale], cwd=work2, expect=expect)
        self.assertEqual([n.text for n in coord.read_notes("text")], ["one"])

    def test_a_racing_append_from_a_second_clone_merges(self):
        """End to end: two clones append to one area; both notes survive."""
        work2 = self._second_clone()
        self.run_coord("note", "--area", "text", "--agent", "a", "--text", "one")
        # The peer appends without having seen our note; append_note re-reads
        # and merges under the compare-and-swap.
        coord.append_note("text", "b", "two", cwd=work2)
        coord.fetch_notes()  # pick up the peer's push through the shared remote
        texts = [n.text for n in coord.read_notes("text")]
        self.assertEqual(sorted(texts), ["one", "two"])


class TestAgentNames(CoordTestCase):
    """A name is `<role>-<n>-<host>` and is never reused (docs/focus-areas.json).

    The name is what makes two same-model workspaces distinguishable, and what
    the Product Owner requests by.  A replacement is a NEW name: a worker starts
    with no memory of its predecessor, so inheriting the name would be a lie.
    """

    def test_parse_valid_and_legacy_names(self):
        self.assertEqual(coord.parse_agent_name("ta-1-kastenbrot"),
                         {"role": "ta", "n": 1, "host": "kastenbrot"})
        self.assertEqual(coord.parse_agent_name("SM-2-Gnampf"),
                         {"role": "sm", "n": 2, "host": "gnampf"})
        # A legacy bare model string is not an agent name (no host).
        self.assertIsNone(coord.parse_agent_name("OpenHands"))
        self.assertIsNone(coord.parse_agent_name("Claude Sonnet 4.5"))
        self.assertEqual(coord.agent_host("OpenHands"), "")
        self.assertEqual(coord.agent_host("po-1-gnampf"), "gnampf")

    def test_register_allocates_the_next_number_per_role_and_host(self):
        out, _ = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "ta", "--model", "m")
        self.assertIn("registered ta-1-kastenbrot", out)
        out, _ = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "ta", "--model", "m")
        self.assertIn("registered ta-2-kastenbrot", out)
        # A different role numbers independently.
        out, _ = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "sm")
        self.assertIn("registered sm-1-kastenbrot", out)
        # A different host numbers independently, and keeps its own roster ref.
        out, _ = self.run_coord("agent", "--register", "--host", "gnampf",
                                "--role", "ta")
        self.assertIn("registered ta-1-gnampf", out)
        self.assertEqual(sorted(self.remote_refs()),
                         ["refs/coord/roster/gnampf", "refs/coord/roster/kastenbrot"])

    def test_a_name_is_never_reused(self):
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "ta")
        self.run_coord("agent", "--retire", "ta-1-kastenbrot", "--host", "kastenbrot",
                       "--succeeded-by", "ta-2-kastenbrot")
        # The next registration must NOT be ta-1 again (retired names stay taken).
        out, _ = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "ta")
        self.assertIn("registered ta-2-kastenbrot", out)
        # And pinning a taken name is refused.
        _, err = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "ta", "--name", "ta-1-kastenbrot",
                                expect=2)
        self.assertIn("never reused", err)

    def test_a_name_for_another_host_is_refused(self):
        _, err = self.run_coord("agent", "--register", "--host", "kastenbrot",
                                "--role", "ta", "--name", "ta-1-gnampf", expect=2)
        self.assertIn("names host 'gnampf'", err)

    def test_retire_records_the_successor(self):
        self.run_coord("agent", "--register", "--host", "gnampf", "--role", "ta",
                       "--name", "ta-1-gnampf")
        self.run_coord("agent", "--register", "--host", "gnampf", "--role", "ta",
                       "--name", "ta-2-gnampf")
        self.run_coord("agent", "--retire", "ta-1-gnampf", "--host", "gnampf",
                       "--succeeded-by", "ta-2-gnampf")
        roster = coord.read_roster("gnampf")
        by_name = {a.name: a for a in roster.agents}
        self.assertEqual(by_name["ta-1-gnampf"].status, "retired")
        self.assertEqual(by_name["ta-1-gnampf"].succeeded_by, "ta-2-gnampf")
        self.assertEqual([a.name for a in roster.active()], ["ta-2-gnampf"])

    def test_hosts_may_carry_a_hyphen_or_dot(self):
        self.assertEqual(coord.parse_agent_name("ta-1-work-2.local"),
                         {"role": "ta", "n": 1, "host": "work-2.local"})
        self.assertTrue(coord.valid_host("work-2.local"))
        self.assertFalse(coord.valid_host(""))
        self.assertFalse(coord.valid_host("bad/../host"))
        _, err = self.run_coord("agent", "--register", "--host", "bad/../host",
                                "--role", "ta", expect=2)
        self.assertIn("not a valid host", err)

    def test_the_host_defaults_to_coord_host_then_the_hostname(self):
        os.environ["COORD_HOST"] = "Kastenbrot"
        self.addCleanup(os.environ.pop, "COORD_HOST", None)
        self.assertEqual(coord.default_host(), "kastenbrot")
        out, _ = self.run_coord("agent", "--register", "--role", "ta")
        self.assertIn("registered ta-1-kastenbrot", out)

    def test_register_counts_against_the_daily_budget(self):
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "ta")
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "wo")
        roster = coord.read_roster("kastenbrot")
        self.assertEqual(roster.budget["started"], 2)
        self.assertEqual(roster.budget["date"], coord.iso(self.now)[:10])

    def test_a_foreign_host_roster_is_separate(self):
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "ta")
        self.run_coord("agent", "--register", "--host", "gnampf", "--role", "ta")
        out, _ = self.run_coord("agent", "--host", "kastenbrot")
        self.assertIn("ta-1-kastenbrot", out)
        self.assertNotIn("gnampf", out)

    def test_status_shows_the_roster(self):
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "sm")
        out, _ = self.run_coord("status")
        self.assertIn("roster kastenbrot", out)
        self.assertIn("sm-1-kastenbrot", out)

    def test_board_renders_the_roster(self):
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "ta")
        content = coord.render_board(at=self.now)
        self.assertIn("## Roster", content)
        self.assertIn("ta-1-kastenbrot", content)

    def test_a_sweep_keeps_the_roster_on_the_board(self):
        # The sweep regenerates and *commits* the board, so it must read the
        # roster refs too: a sweep that read only the leases would drop the
        # Roster section and commit the loss.
        self.run_coord("agent", "--register", "--host", "kastenbrot", "--role", "ta")
        self.run_coord("note", "--area", "sm", "--agent", "sm-1-kastenbrot",
                       "--text", "handoff")
        self.run_coord("sweep", "--no-board")
        content = coord.render_board(at=self.now)
        self.assertIn("ta-1-kastenbrot", content)
        self.assertIn("handoff", content)


class TestAgentCommand(CoordTestCase):
    def test_list_is_accepted(self):
        self.run_coord("agent", "--register", "--host", "h", "--role", "ta")
        out, _ = self.run_coord("agent", "--list")
        self.assertIn("ta-1-h", out)

    def test_an_unknown_role_code_fails_fast(self):
        _, err = self.run_coord("agent", "--register", "--host", "h", "--role", "zz",
                                expect=2)
        self.assertIn("unknown role code", err)

    def test_a_pinned_name_must_match_the_host(self):
        _, err = self.run_coord("agent", "--register", "--host", "h1", "--role", "ta",
                                "--name", "ta-9-h2", expect=2)
        self.assertIn("names host 'h2'", err)

    def test_a_pinned_name_is_never_reused(self):
        self.run_coord("agent", "--register", "--host", "h", "--role", "ta",
                       "--name", "ta-7-h")
        _, err = self.run_coord("agent", "--register", "--host", "h", "--role", "ta",
                                "--name", "ta-7-h", expect=2)
        self.assertIn("never reused", err)


class TestTakeoverRequests(CoordTestCase):
    """A takeover request is the cooperative, low-cost reclamation signal."""

    def test_request_is_recorded_on_the_channel(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        out, _ = self.run_coord("takeover", "--request", "--area", "world",
                                "--agent", "sm-1-kastenbrot")
        self.assertIn("takeover requested for world", out)
        out, _ = self.run_coord("takeover")
        self.assertIn("TAKEOVER REQUEST", out)
        self.assertIn("holder=a", out)

    def test_request_on_a_free_area_is_a_noop(self):
        out, _ = self.run_coord("takeover", "--request", "--area", "world",
                                "--agent", "a")
        self.assertIn("is free", out)

    def test_next_surfaces_a_request_for_a_now_free_area(self):
        # A request filed while held, whose holder then released: the next idle
        # agent is told to pick the work up (the whole point of the channel).
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        self.run_coord("takeover", "--request", "--area", "world", "--agent", "sm-1-kastenbrot")
        self.run_coord("release", "--area", "world", "--issue", 157, "--agent", "a")
        self._stub_issues({})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "b")
        self.assertIn("Takeover request on 'world'", out)

    def test_a_note_on_a_channel_is_not_a_lease(self):
        self.run_coord("note", "--area", "sm", "--agent", "sm-1-kastenbrot",
                       "--text", "handoff: nothing in flight")
        self.assertEqual(self.remote_refs(), ["refs/coord/notes/sm"])
        self.run_coord("check")
        out, _ = self.run_coord("notes", "--area", "sm")
        self.assertIn("nothing in flight", out)

    def test_po_channel_is_available(self):
        self.run_coord("note", "--area", "po", "--agent", "po-1-kastenbrot",
                       "--text", "priority: #405 then #406")
        out, _ = self.run_coord("notes", "--area", "po")
        self.assertIn("priority: #405 then #406", out)


class TestFocusConfig(CoordTestCase):
    """Focus areas are data (docs/focus-areas.json); the code is only a fallback."""

    def test_default_config_matches_the_shipped_file(self):
        # The shipped file and the built-in fallback must agree, so a redesign
        # that edits only one of them is caught.
        self.assertEqual(tuple(coord.CONFIG.areas), tuple(coord._DEFAULT_AREAS))
        self.assertEqual(coord.CONFIG.critical_path_areas,
                         ("tactical", "metagame", "world"))
        for name, spec in coord._DEFAULT_AREAS.items():
            self.assertEqual(coord.CONFIG.areas[name]["code"], spec["code"])

    def test_role_codes_are_unique(self):
        codes = [s["code"] for s in coord.CONFIG.areas.values()]
        codes += [s["code"] for s in coord.CONFIG.roles.values()]
        self.assertEqual(len(codes), len(set(codes)), f"duplicate role code in {codes}")

    def test_a_missing_file_falls_back_without_raising(self):
        cfg = coord.load_focus_config("/nonexistent/focus-areas.json")
        self.assertEqual(tuple(cfg.areas), tuple(coord._DEFAULT_AREAS))
        self.assertEqual(cfg.critical_path_areas,
                         ("tactical", "metagame", "world"))

    def test_a_redesigned_file_changes_the_area_set(self):
        path = os.path.join(self.tmp, "focus.json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({
                "areas": {"alpha": {"code": "al"}, "beta": {"code": "be"}},
                "roles": {"sm": {"label": "Scrum master"}},
                "channels": {"sm": "handoff"},
                "critical_path": ["beta"],
            }, fh)
        cfg = coord.load_focus_config(path)
        self.assertEqual(cfg.area_names, ("alpha", "beta"))
        self.assertEqual(cfg.critical_path_areas, ("beta",))
        self.assertEqual(cfg.code_for("beta"), "be")

    def test_a_corrupt_file_falls_back_without_raising(self):
        path = os.path.join(self.tmp, "focus.json")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("{not json")
        cfg = coord.load_focus_config(path)
        self.assertEqual(tuple(cfg.areas), tuple(coord._DEFAULT_AREAS))

    def test_the_shipped_file_is_valid_json(self):
        path = os.path.join(REPO_ROOT, "docs", "focus-areas.json")
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
        self.assertIn("areas", data)
        self.assertIn("critical_path", data)
        self.assertIn("sm", data["roles"])
        self.assertIn("po", data["roles"])

    def test_the_critical_path_matches_the_roadmap(self):
        # The critical path is data, but it is *not* free: it must agree with the
        # roadmap's long pole, or a redesign silently reorders the fleet.
        with open(os.path.join(REPO_ROOT, "docs", "ROADMAP.md"), encoding="utf-8") as fh:
            roadmap = fh.read()
        self.assertIn("Milestone B", roadmap)
        self.assertIn("Milestone C", roadmap)
        self.assertIn("metagame", coord.CONFIG.critical_path_areas)
        # Milestone C: the mission set (tactical) is the long pole, so it leads
        # (issue #425).  A redesign that puts a delivery area first would starve
        # the feature lanes.
        self.assertEqual(coord.CONFIG.critical_path_areas[0], "tactical")

    def test_delivery_areas_stay_off_the_critical_path(self):
        # platform/render/release "do delivery, not features" (FOCUS_AREAS §2):
        # they stay claimable at rung 4 but must not absorb critical-path
        # capacity from the feature lanes (issue #425).
        for area in ("platform", "render", "release"):
            self.assertNotIn(area, coord.CONFIG.critical_path_areas)

    def test_sm_owned_areas_are_real_areas_and_excluded_from_worker_routing(self):
        # The SM-owned areas are infrastructure, not game content: they must be
        # real areas (a typo would silently hide nothing), and a plain worker
        # must never be routed to them.
        for area in coord.SM_OWNED_AREAS:
            self.assertIn(area, coord.AREAS)
        for area in coord.SM_OWNED_AREAS:
            self.assertNotIn(area, coord.WORKER_AREAS)
        self.assertIn("platform", coord.SM_OWNED_AREAS)
        self.assertIn("rules", coord.SM_OWNED_AREAS)
        self.assertNotIn("tactical", coord.SM_OWNED_AREAS)

    def test_a_stale_sm_owned_entry_is_dropped_not_trusted(self):
        path = os.path.join(self.tmp, "focus.json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({
                "areas": {"alpha": {"code": "al"}},
                "sm_owned": ["alpha", "does-not-exist"],
            }, fh)
        cfg = coord.load_focus_config(path)
        self.assertEqual(cfg.sm_owned, ("alpha",))
        self.assertEqual(cfg.worker_areas, ())

    def test_missing_sm_owned_falls_back_to_the_default(self):
        path = os.path.join(self.tmp, "focus.json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({"areas": {"alpha": {"code": "al"}}}, fh)
        cfg = coord.load_focus_config(path)
        self.assertEqual(cfg.sm_owned, coord._DEFAULT_SM_OWNED)

    def test_the_shipped_file_declares_sm_owned_areas(self):
        path = os.path.join(REPO_ROOT, "docs", "focus-areas.json")
        with open(path, encoding="utf-8") as fh:
            data = json.load(fh)
        self.assertEqual(data.get("sm_owned"), ["platform", "rules"])
        self.assertIn("rules", data["areas"])


class TestSmOwnedRouting(CoordTestCase):
    """SM-owned areas are triggered by the SM, never scheduled for a worker."""

    def test_a_worker_is_never_routed_to_an_sm_owned_area(self):
        # Only `platform` has work; a plain worker must not be offered it.
        self._stub_issues({"platform": [(347, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "ta-1-kastenbrot")
        self.assertNotIn("claim platform", out)
        self.assertIn("No open issue is claimable", out)

    def test_an_sm_is_routed_to_its_own_area(self):
        self._stub_issues({"platform": [(347, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "sm-1-kastenbrot")
        self.assertIn("claim platform / #347", out)

    def test_an_sm_prefers_its_own_area_over_the_critical_path(self):
        # An idle SM triggers its own infrastructure work rather than competing
        # with a worker for the critical path.
        self._stub_issues({"platform": [(347, False)], "tactical": [(405, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "sm-1-kastenbrot")
        self.assertIn("claim platform / #347", out)
        self.assertNotIn("claim tactical", out)

    def test_a_worker_is_still_routed_to_the_critical_path(self):
        self._stub_issues({"tactical": [(405, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "ta-1-kastenbrot")
        self.assertIn("claim tactical / #405", out)
        self.assertIn("critical path", out)

    def test_a_legacy_bare_name_is_not_treated_as_an_sm(self):
        # A legacy model-string agent has no role, so it is a worker: it must not
        # be routed to an SM-owned area.
        self._stub_issues({"platform": [(347, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "OpenHands")
        self.assertNotIn("claim platform", out)

    def test_an_sm_with_no_infrastructure_issue_is_not_routed_to_a_game_area(self):
        # The SM owns no game area: with no SM-area issue it falls through to its
        # other durable jobs, never to a worker's area.
        self._stub_issues({"tactical": [(405, False)]})
        self._stub_prs([])
        out, _ = self.run_coord("next", "--agent", "sm-1-kastenbrot")
        self.assertNotIn("claim tactical", out)
        self.assertNotIn("claim platform", out)


class TestDirectMessages(CoordTestCase):
    """After the initial prompt, agents talk to each other directly."""

    def test_a_message_is_delivered_to_one_named_agent(self):
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent",
                       "po-2-kastenbrot", "--text", "use the escort mission")
        messages = coord.read_messages("ta-1-kastenbrot")
        self.assertEqual([m.text for m in messages], ["use the escort mission"])
        self.assertEqual([m.agent for m in messages], ["po-2-kastenbrot"])

    def test_a_message_goes_to_the_recipient_only(self):
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "for ta")
        self.assertEqual(coord.read_messages("ta-1-kastenbrot")[0].text, "for ta")
        self.assertEqual(coord.read_messages("wo-1-kastenbrot"), [])

    def test_messages_are_pushed_to_the_remote(self):
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "x")
        self.assertIn("refs/coord/msg/ta-1-kastenbrot", self.remote_refs())

    def test_a_message_to_an_invalid_name_is_refused(self):
        out, err = self.run_coord("msg", "--to", "not-a-name", "--agent", "po-2-kastenbrot",
                                  "--text", "x", expect=2)
        self.assertIn("not a valid agent name", err)
        self.assertEqual(coord.read_messages("not-a-name"), [])

    def test_inbox_lists_only_your_own_messages(self):
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "yours")
        self.run_coord("msg", "--to", "wo-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "not yours")
        out, _ = self.run_coord("inbox", "--agent", "ta-1-kastenbrot")
        self.assertIn("yours", out)
        self.assertNotIn("not yours", out)

    def test_inbox_reports_when_empty(self):
        out, _ = self.run_coord("inbox", "--agent", "ta-1-kastenbrot")
        self.assertIn("no messages", out)

    def test_inbox_json_is_machine_readable(self):
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "j")
        out, _ = self.run_coord("inbox", "--agent", "ta-1-kastenbrot", "--json")
        self.assertEqual(json.loads(out)[0]["text"], "j")

    def test_messages_are_capped_to_the_most_recent(self):
        for i in range(coord.MAX_NOTES + 3):
            self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent",
                           "po-2-kastenbrot", "--text", f"m{i}")
        messages = coord.read_messages("ta-1-kastenbrot")
        self.assertEqual(len(messages), coord.MAX_NOTES)
        self.assertEqual(messages[-1].text, f"m{coord.MAX_NOTES + 2}")

    def test_a_message_is_not_a_note_on_an_area(self):
        # A direct message must not leak into the area note logs the board prints.
        self.run_coord("msg", "--to", "ta-1-kastenbrot", "--agent", "po-2-kastenbrot",
                       "--text", "private")
        self.assertEqual(coord.read_notes("tactical"), [])
        self.assertNotIn("private", coord.render_board())


if __name__ == "__main__":
    unittest.main(verbosity=2)
