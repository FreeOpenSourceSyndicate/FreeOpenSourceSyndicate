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
        lease = coord.read_lease("refs/coord/leases/world")
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

    These tests stub the GitHub calls (`_open_issues_by_area`) so they run with
    no network, which is also the real fallback path.
    """

    def _stub_issues(self, mapping):
        self._orig = coord._open_issues_by_area
        coord._open_issues_by_area = lambda: dict(mapping)
        self.addCleanup(setattr, coord, "_open_issues_by_area", self._orig)

    def test_next_returns_your_own_lease_first(self):
        self.run_coord("claim", "--area", "world", "--issue", 157, "--agent", "a")
        self._stub_issues({})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("You hold world / #157", out)
        self.assertIn("renew", out)

    def test_next_prefers_the_critical_path(self):
        self._stub_issues({"metagame": [249], "world": [157]})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("metagame / #249", out)
        self.assertIn("critical path", out)

    def test_next_falls_back_to_any_free_area_with_work(self):
        # metagame held, so the critical-path rung is skipped.
        self.run_coord("claim", "--area", "metagame", "--issue", 249, "--agent", "x")
        self._stub_issues({"metagame": [249], "world": [157]})
        out, _ = self.run_coord("next", "--agent", "a")
        self.assertIn("world / #157", out)

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
        for mapping in ({}, {"world": [1]}, {"metagame": [249]}):
            self._stub_issues(mapping)
            out, _ = self.run_coord("next", "--agent", "a")
            self.assertTrue(out.strip(), f"empty output for {mapping}")


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


if __name__ == "__main__":
    unittest.main(verbosity=2)
