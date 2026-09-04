"""Unit tests for the executed-checks assertion in run_validation.py.

A <check> only runs when its event's <condition> becomes true. A run that
diverges early never reaches the condition, the checks never execute, and the
run used to report no failures. These tests cover the script parser, the
matching of expected check events against the EVENT lines JSBSim prints, and
the way the result reaches the report.

Run with: python -m unittest test_check_events
"""

import os
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_validation as rv  # noqa: E402


SCRIPT = textwrap.dedent("""\
    <?xml version="1.0"?>
    <runscript name="t">
      <use aircraft="x" initialize="i"/>
      <run start="0" end="10" dt="0.01">
        <event name="Setup">
          <condition>simulation/sim-time-sec ge 1.0</condition>
          <set name="a" value="1"/>
          <notify><property>a</property></notify>
        </event>
        <event name="Check A">
          <condition>simulation/sim-time-sec ge 2.0</condition>
          <notify>
            <check property="p" value="1"/>
            <check property="q" value="2" tol_abs="0.1"/>
          </notify>
        </event>
        <event name="Check B" persistent="true">
          <condition>simulation/sim-time-sec ge 3.0</condition>
          <notify><check property="p" value="1"/></notify>
        </event>
      </run>
    </runscript>
    """)


def write_script(text):
    fd, path = tempfile.mkstemp(suffix=".xml")
    with os.fdopen(fd, "w") as f:
        f.write(text)
    return Path(path)


class ParseScriptCheckEvents(unittest.TestCase):
    def test_only_events_with_checks_are_listed(self):
        path = write_script(SCRIPT)
        events = rv.parse_script_check_events(path)
        self.assertEqual([(e.name, e.checks, e.repeating) for e in events],
                         [("Check A", 2, False), ("Check B", 1, True)])

    def test_continuous_events_repeat(self):
        path = write_script(SCRIPT.replace('persistent="true"', 'continuous="true"'))
        events = rv.parse_script_check_events(path)
        self.assertTrue(events[1].repeating)

    def test_duplicate_check_event_names_are_a_script_error(self):
        path = write_script(SCRIPT.replace('name="Check B"', 'name="Check A"'))
        with self.assertRaises(ValueError):
            rv.parse_script_check_events(path)

    def test_unnamed_check_event_is_a_script_error(self):
        path = write_script(SCRIPT.replace('<event name="Check B" persistent="true">',
                                           '<event persistent="true">'))
        with self.assertRaises(ValueError):
            rv.parse_script_check_events(path)

    def test_script_without_checks_gives_empty_list(self):
        path = write_script(SCRIPT.replace("<check", "<nocheck"))
        self.assertEqual(rv.parse_script_check_events(path), [])


STDOUT = textwrap.dedent("""\
    Setup (Event 0) executed at time:   1.000000
        a = 1
    Check A (Event 1) executed at time:   2.000000
        CHECK PASS: p = 1 expected 1 tol 1e-06
        CHECK FAIL: q = 2.5 expected 2 tol 0.1
      EVENT FAIL: Check A (1 of 2 checks failed)

    Check B (Event 2) executed at time:   3.000000
        CHECK PASS: p = 1 expected 1 tol 1e-06
      EVENT PASS: Check B (1 of 1 checks passed)

    Check B (Event 2) executed at time:   3.010000
        CHECK PASS: p = 1 expected 1 tol 1e-06
      EVENT PASS: Check B (1 of 1 checks passed)

    """)


class ParseEventLines(unittest.TestCase):
    def test_event_lines_carry_name_and_check_total(self):
        parsed = rv.parse_testplane_output(STDOUT)
        ev = parsed["event_results"]
        self.assertEqual([(e["name"], e["passed"], e["total"]) for e in ev],
                         [("Check A", False, 2), ("Check B", True, 1), ("Check B", True, 1)])

    def test_or_mode_pass_line_is_parsed(self):
        out = "  EVENT PASS: Either (1 of 2 checks passed, or-mode)\n"
        ev = rv.parse_testplane_output(out)["event_results"]
        self.assertEqual((ev[0]["name"], ev[0]["passed"], ev[0]["total"]), ("Either", True, 2))


def expected():
    return [rv.ScriptCheckEvent("Check A", 2, False),
            rv.ScriptCheckEvent("Check B", 1, True)]


def fired(name, total, times=1, passed=True):
    return [{"name": name, "passed": passed, "count": total, "total": total, "time": 1.0}
            for _ in range(times)]


class MatchCheckEvents(unittest.TestCase):
    def test_every_event_fired_once_passes(self):
        results = rv.match_check_events(expected(), fired("Check A", 2) + fired("Check B", 1))
        self.assertTrue(all(r.passed for r in results))
        self.assertEqual([(r.name, r.fired) for r in results], [("Check A", 1), ("Check B", 1)])

    def test_event_that_never_fired_fails(self):
        results = rv.match_check_events(expected(), fired("Check B", 1))
        a = results[0]
        self.assertFalse(a.passed)
        self.assertEqual(a.fired, 0)
        self.assertIn("never fired", a.message)

    def test_repeating_event_may_fire_many_times(self):
        results = rv.match_check_events(expected(), fired("Check A", 2) + fired("Check B", 1, times=5))
        self.assertTrue(results[1].passed)
        self.assertEqual(results[1].fired, 5)

    def test_one_shot_event_firing_twice_fails(self):
        results = rv.match_check_events(expected(), fired("Check A", 2, times=2) + fired("Check B", 1))
        self.assertFalse(results[0].passed)
        self.assertIn("2 times", results[0].message)

    def test_wrong_number_of_checks_in_a_firing_fails(self):
        results = rv.match_check_events(expected(), fired("Check A", 1) + fired("Check B", 1))
        self.assertFalse(results[0].passed)
        self.assertIn("1 of 2", results[0].message)

    def test_check_failure_inside_the_event_does_not_change_firing_status(self):
        # A failed check is reported by the check itself; the event still fired.
        results = rv.match_check_events(expected(), fired("Check A", 2, passed=False) + fired("Check B", 1))
        self.assertTrue(results[0].passed)


class ResultAndReport(unittest.TestCase):
    def make_result(self, event_results):
        tr = rv.TestResult(rv.TestSpec("t1", "grp/t1.xml"))
        tr.check_event_results = rv.match_check_events(expected(), event_results)
        return tr

    def test_unfired_event_fails_the_test(self):
        tr = self.make_result(fired("Check B", 1))
        self.assertFalse(tr.passed)

    def test_all_fired_passes_the_test(self):
        tr = self.make_result(fired("Check A", 2) + fired("Check B", 1))
        self.assertTrue(tr.passed)

    def test_report_names_the_unfired_event(self):
        tr = self.make_result(fired("Check B", 1))
        report = rv.format_report([tr], "X")
        self.assertIn("[FAIL] t1", report)
        self.assertIn("Check A", report)
        self.assertIn("never fired", report)

    def test_report_counts_fired_events_on_pass(self):
        tr = self.make_result(fired("Check A", 2) + fired("Check B", 1))
        report = rv.format_report([tr], "X")
        self.assertIn("[PASS] t1", report)
        self.assertIn("2/2 check events fired", report)

    def test_suite_with_expected_events_but_none_fired_is_flagged(self):
        tr = self.make_result([])
        report = rv.format_report([tr], "X")
        self.assertIn("no check event fired in any test", report)


if __name__ == "__main__":
    unittest.main()
