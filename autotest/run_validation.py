#!/usr/bin/env python3
"""
aceFM QTG-Style Validation Runner

Runs JSBSim autotest scripts via JSBSim.exe, compares output CSVs against
baseline data in a validation package, and reports pass/fail per-property
using tolerances defined in validation-package/tolerances.xml.

Adapted from JSBSim's check_cases pattern (JSBSim_utils.py).

Requires autotest/autotest.xml (master config) and
autotest/validation-package/tolerances.xml in the aircraft mod directory.

Usage:
    # Run all tests (JSBSim.exe found via the build tree or PATH)
    python run_validation.py --aircraft path/to/AircraftMod

    # Run all tests with an explicit JSBSim.exe path
    python run_validation.py --jsbsim path/to/JSBSim.exe --aircraft path/to/AircraftMod

    # FlightGear path layout, or the bridge model set
    python run_validation.py --aircraft path/to/AircraftMod --mode fg
    python run_validation.py --aircraft path/to/AircraftMod --mode bridge

    # Run a specific test
    python run_validation.py --aircraft path/to/AircraftMod level_accel

    # Promote current output to baseline (after intentional model change)
    python run_validation.py --aircraft path/to/AircraftMod --promote level_accel

    # Promote all tests
    python run_validation.py --aircraft path/to/AircraftMod --promote
"""

import argparse
import fnmatch
import math
import os
import re
import shutil
import subprocess
import sys
from collections import OrderedDict
from datetime import datetime
from pathlib import Path
from xml.dom import minidom
from xml.etree import ElementTree as ET

import numpy as np
import pandas as pd


_JSBSIM_PREFIXES = ("fdm/jsbsim/", "/fdm/jsbsim/")


def strip_jsbsim_prefix(name):
    """Strip the leading fdm/jsbsim/ prefix from a JSBSim property path."""
    for pfx in _JSBSIM_PREFIXES:
        if name.startswith(pfx):
            return name[len(pfx):]
    return name


class ToleranceSpec:
    """Tolerance specification for a single property."""

    def __init__(self, name, tol_abs=None, tol_rel=None):
        self.name = name
        self.tol_abs = float(tol_abs) if tol_abs is not None else None
        self.tol_rel = float(tol_rel) if tol_rel is not None else None


class GlobalTolerances:
    """Global tolerance rules parsed from <global-tolerances>."""

    def __init__(self):
        self.rules = []  # list of (pattern, ToleranceSpec)
        self.default_tol_abs = None
        self.default_tol_rel = None

    def match(self, prop_name):
        """Find the first matching global rule for a property name."""
        for pattern, tol_spec in self.rules:
            if fnmatch.fnmatch(prop_name, pattern):
                return ToleranceSpec(prop_name, tol_spec.tol_abs, tol_spec.tol_rel)
        return None

    def get_default(self, prop_name):
        """Return the global default tolerance, if defined."""
        if self.default_tol_abs is not None or self.default_tol_rel is not None:
            return ToleranceSpec(prop_name, self.default_tol_abs, self.default_tol_rel)
        return None


class OutputFileSpec:
    """Specification for one output file comparison within a test."""

    def __init__(self, output_name, baseline_name):
        self.output_name = output_name
        self.baseline_name = baseline_name
        self.properties = {}  # name -> ToleranceSpec
        self.default_tol_abs = None
        self.default_tol_rel = None
        self.global_tolerances = None  # GlobalTolerances reference

    def get_tolerance(self, prop_name):
        """Get tolerance for a property.

        Lookup order:
        1. Per-test <property> override (exact match)
        2. Global tolerance rules (glob pattern match, first match wins)
        3. Per-test <default>
        4. Global <default>
        """
        if prop_name in self.properties:
            return self.properties[prop_name]
        if self.global_tolerances:
            gt = self.global_tolerances.match(prop_name)
            if gt:
                return gt
        if self.default_tol_abs is not None or self.default_tol_rel is not None:
            return ToleranceSpec(prop_name, self.default_tol_abs, self.default_tol_rel)
        if self.global_tolerances:
            gd = self.global_tolerances.get_default(prop_name)
            if gd:
                return gd
        return None


class TestSpec:
    """Specification for a single validation test."""

    def __init__(self, name, script, description=""):
        self.name = name
        self.script = script
        self.description = description
        self.data_source_level = ""
        self.output_files = []  # list of OutputFileSpec


class PropertyResult:
    """Result of comparing a single property."""

    def __init__(self, name, max_delta, tol_abs, tol_rel, baseline_at_max, passed):
        self.name = name
        self.max_delta = max_delta
        self.tol_abs = tol_abs
        self.tol_rel = tol_rel
        self.baseline_at_max = baseline_at_max
        self.passed = passed

    @property
    def pct_err(self):
        if self.baseline_at_max and abs(self.baseline_at_max) > 1e-12:
            return abs(self.max_delta / self.baseline_at_max) * 100.0
        return None


class CheckResult:
    """Result of a single <check> evaluation from JSBSim stdout."""

    def __init__(self, property, actual, expected, tolerance, passed, message="",
                 time=None):
        self.property = property
        self.actual = actual
        self.expected = expected
        self.tolerance = tolerance
        self.passed = passed
        self.message = message
        self.time = time  # simulation time when the check was evaluated


class TestResult:
    """Result of running and validating a single test."""

    def __init__(self, test_spec):
        self.test_spec = test_spec
        self.property_results = []
        self.check_results = []  # list of CheckResult from <check> elements
        self.run_return_code = 0  # TestPlane.exe return code (0=pass, 1=check-fail)
        self.run_error = None
        self.stdout = ""

    @property
    def passed(self):
        if self.run_error:
            return False
        csv_ok = all(pr.passed for pr in self.property_results)
        check_ok = all(cr.passed for cr in self.check_results)
        rc_ok = self.run_return_code == 0
        return csv_ok and check_ok and rc_ok

    @property
    def num_passed(self):
        return sum(1 for pr in self.property_results if pr.passed)

    @property
    def num_failed(self):
        return sum(1 for pr in self.property_results if not pr.passed)

    @property
    def num_total(self):
        return len(self.property_results)

    @property
    def failures(self):
        return [pr for pr in self.property_results if not pr.passed]


def parse_tolerances(tolerances_path):
    """Parse validation-package/tolerances.xml — tolerances only.

    Returns (GlobalTolerances, dict of per-test tolerance overrides).
    The per-test dict maps test_name -> {baseline_name -> OutputFileSpec}.
    """
    tree = ET.parse(tolerances_path)
    root = tree.getroot()

    # Parse <global-tolerances> section
    global_tols = None
    gt_elem = root.find("global-tolerances")
    if gt_elem is not None:
        global_tols = GlobalTolerances()
        for tol_elem in gt_elem.findall("tolerance"):
            pattern = tol_elem.get("match")
            tol = ToleranceSpec(
                pattern,
                tol_abs=tol_elem.get("tol_abs"),
                tol_rel=tol_elem.get("tol_rel"),
            )
            global_tols.rules.append((pattern, tol))
        gt_default = gt_elem.find("default")
        if gt_default is not None:
            global_tols.default_tol_abs = float(gt_default.get("tol_abs", "1e-3"))
            global_tols.default_tol_rel = float(gt_default.get("tol_rel", "0.01"))

    # Parse per-test tolerance overrides
    per_test_overrides = {}
    for test_elem in root.findall("test"):
        name = test_elem.get("name")
        overrides = {}
        for of_elem in test_elem.findall("output-file"):
            baseline_name = of_elem.get("baseline")
            of_spec = OutputFileSpec("", baseline_name)
            of_spec.global_tolerances = global_tols

            for prop_elem in of_elem.findall("property"):
                prop_name = prop_elem.get("name")
                tol = ToleranceSpec(
                    prop_name,
                    tol_abs=prop_elem.get("tol_abs"),
                    tol_rel=prop_elem.get("tol_rel"),
                )
                of_spec.properties[prop_name] = tol

            default_elem = of_elem.find("default")
            if default_elem is not None:
                of_spec.default_tol_abs = float(default_elem.get("tol_abs", "1e-3"))
                of_spec.default_tol_rel = float(default_elem.get("tol_rel", "0.01"))

            overrides[baseline_name] = of_spec
        if overrides:
            per_test_overrides[name] = overrides

    return global_tols, per_test_overrides


def parse_autotest_config(autotest_xml_path, tolerances_path):
    """Parse autotest.xml master config and return (paths_dict, list of TestSpec).

    Merges tolerance overrides from tolerances.xml into the test specs.
    Also parses test-definition.xml metadata for each group.
    """
    tree = ET.parse(autotest_xml_path)
    root = tree.getroot()
    autotest_dir = Path(autotest_xml_path).parent

    # Suite-level attributes (from <autotest-suite aircraft="..." company="..." logo="...">)
    suite_aircraft = root.get("aircraft", "")
    suite_metadata = {}
    if root.get("company"):
        suite_metadata["company"] = root.get("company")
    if root.get("logo"):
        suite_metadata["logo"] = root.get("logo")

    # Parse <paths>
    paths = {}
    paths_elem = root.find("paths")
    if paths_elem is not None:
        for child in paths_elem:
            paths[child.tag] = child.text.strip() if child.text else ""

    # Parse tolerances
    global_tols, per_test_overrides = parse_tolerances(tolerances_path)

    # Parse test-definition.xml metadata per group
    group_metadata = {}

    tests = []
    for group_elem in root.findall("test-group"):
        group_name = group_elem.get("name", "")
        definition_path = group_elem.get("definition", "")

        # Load group metadata from test-definition.xml
        if definition_path:
            td_path = autotest_dir / definition_path
            if td_path.exists():
                try:
                    td_tree = ET.parse(td_path)
                    td_root = td_tree.getroot()
                    meta = {}
                    for field in _TEST_DEF_FIELDS:
                        elem = td_root.find(field)
                        if elem is not None and elem.text:
                            meta[field] = elem.text.strip()
                    # Inject suite-level aircraft name if not in definition
                    if "aircraft" not in meta and suite_aircraft:
                        meta["aircraft"] = suite_aircraft
                    # Rewrite doc_number prefix from TN- to TR-
                    if "doc_number" in meta:
                        dn = meta["doc_number"]
                        if dn.startswith("TN-") or dn.startswith("TN_"):
                            meta["doc_number"] = "TR-" + dn[3:]
                    if meta:
                        group_metadata[group_name] = meta
                except ET.ParseError:
                    pass

        for test_elem in group_elem.findall("test"):
            name = test_elem.get("name")
            script = test_elem.get("script")
            desc_elem = test_elem.find("description")
            description = desc_elem.text.strip() if desc_elem is not None and desc_elem.text else ""

            spec = TestSpec(name, script, description)

            ds_elem = test_elem.find("data-source")
            if ds_elem is not None:
                spec.data_source_level = ds_elem.get("level", "")

            for of_elem in test_elem.findall("output-file"):
                output_name = of_elem.get("name")
                baseline_name = of_elem.get("baseline")
                of_spec = OutputFileSpec(output_name, baseline_name)
                of_spec.global_tolerances = global_tols

                # Merge per-test tolerance overrides from tolerances.xml
                if name in per_test_overrides:
                    override = per_test_overrides[name].get(baseline_name)
                    if override:
                        of_spec.properties = override.properties.copy()
                        if override.default_tol_abs is not None:
                            of_spec.default_tol_abs = override.default_tol_abs
                        if override.default_tol_rel is not None:
                            of_spec.default_tol_rel = override.default_tol_rel

                spec.output_files.append(of_spec)

            tests.append(spec)

    return paths, tests, group_metadata, suite_metadata


# Path conventions for a script run, expressed as JSBSim command-line options.
# These were previously baked into TestPlane's test / test-fg / bridge-test
# modes; they live here now so the runner drives stock JSBSim.exe directly.
_PATH_CONVENTIONS = {
    "dcs": ["--root=.", "--aircraft-path=EFM", "--engine-path=EFM/Engines",
            "--systems-path=EFM/Systems", "--init-path=autotest/init"],
    "fg": ["--root=.", "--aircraft-path=.", "--engine-path=Engines",
           "--systems-path=Systems", "--init-path=autotest/init"],
    "bridge": ["--root=.", "--aircraft-path=EFM", "--engine-path=EFM/Engines",
               "--systems-path=EFM/Systems", "--init-path=autotest/init",
               "--property=simulation/models/propagate/enabled=0",
               "--property=simulation/models/groundreactions/enabled=0"],
}


def resolve_jsbsim_exe(args):
    """Locate the standalone JSBSim.exe script runner.

    Resolution order, first existing wins: --jsbsim, JSBSIM_EXE, alongside
    --testplane, the local CMake build tree, then PATH. Exits with guidance
    if none is found.
    """
    candidates = []
    if args.jsbsim:
        candidates.append(Path(args.jsbsim))
    if os.environ.get("JSBSIM_EXE"):
        candidates.append(Path(os.environ["JSBSIM_EXE"]))
    if args.testplane:
        candidates.append(Path(args.testplane).resolve().parent / "JSBSim.exe")
    repo_root = Path(__file__).resolve().parent.parent
    for cfg in ("Release", "Debug"):
        candidates.append(repo_root / "build" / "JSBSim" / "src" / cfg / "JSBSim.exe")

    for c in candidates:
        if c and c.is_file():
            return c.resolve()

    found = shutil.which("JSBSim") or shutil.which("JSBSim-bin")
    if found:
        return Path(found).resolve()

    tried = "\n  ".join(str(c) for c in candidates) or "(none)"
    print("ERROR: JSBSim.exe not found. Tried:\n  " + tried +
          "\nSpecify with --jsbsim or set JSBSIM_EXE.")
    sys.exit(2)


def run_test(jsbsim_exe, aircraft_dir, script_path, mode="dcs", verbose=0):
    """Run a script test via JSBSim.exe and return (returncode, stdout, stderr).

    mode selects the path convention and model set:
        dcs    — DCS mod layout (EFM/, EFM/Engines, EFM/Systems)
        fg     — FlightGear layout (., Engines, Systems)
        bridge — DCS layout with Propagate and GroundReactions disabled,
                 reproducing the model set the EFM bridge runs.

    verbose >= 1 prints the JSBSim command line; verbose >= 2 also prints
    the working directory.

    Check failures are reported by JSBSim on stdout and detected by the
    caller's parse; they do not necessarily set a non-zero return code.
    """
    conv = _PATH_CONVENTIONS.get(mode)
    if conv is None:
        return -1, "", f"unknown mode '{mode}'"
    cmd = [str(jsbsim_exe), f"--script={script_path}"] + conv
    if verbose >= 1:
        print("    cmd: " + subprocess.list2cmdline(cmd))
        if verbose >= 2:
            print(f"    cwd: {aircraft_dir}")
    env = dict(os.environ)
    env.setdefault("JSBSIM_DEBUG", "0")
    try:
        result = subprocess.run(
            cmd,
            cwd=str(aircraft_dir),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=300,
            env=env,
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "JSBSim.exe timed out after 300 seconds"
    except FileNotFoundError:
        return -1, "", f"JSBSim.exe not found at {jsbsim_exe}"


def check_property(output_col, baseline_col, tol_spec):
    """
    Compare a single property column between output and baseline.
    Returns a PropertyResult.

    Pass condition: max_delta <= tol_abs OR max_delta/|baseline| <= tol_rel
    (satisfying either is sufficient).
    """
    delta = np.abs(output_col - baseline_col)
    max_delta = delta.max()
    idx_max = delta.idxmax()
    baseline_at_max = baseline_col.loc[idx_max]

    passed = False
    tol_abs = tol_spec.tol_abs if tol_spec else None
    tol_rel = tol_spec.tol_rel if tol_spec else None

    if tol_abs is not None and max_delta <= tol_abs:
        passed = True

    if not passed and tol_rel is not None:
        # Use near-zero floor to avoid division issues
        ref_magnitude = max(abs(baseline_col).max(), 1e-12)
        if max_delta / ref_magnitude <= tol_rel:
            passed = True

    # If no tolerance was specified at all, we can't validate - mark as fail
    if tol_abs is None and tol_rel is None:
        passed = False

    return PropertyResult(
        name=tol_spec.name if tol_spec else output_col.name,
        max_delta=max_delta,
        tol_abs=tol_abs,
        tol_rel=tol_rel,
        baseline_at_max=baseline_at_max,
        passed=passed,
    )


def compare_csv(output_path, baseline_path, of_spec):
    """
    Compare an output CSV against its baseline using tolerances from of_spec.
    Returns list of PropertyResult.

    Adapted from JSBSim's isDataMatching() and FindDifferences().
    """
    results = []

    try:
        output_df = pd.read_csv(output_path)
        baseline_df = pd.read_csv(baseline_path)
    except Exception as e:
        # Return a single failing result for file-level errors
        pr = PropertyResult(
            name="<file-read>",
            max_delta=float("inf"),
            tol_abs=0,
            tol_rel=0,
            baseline_at_max=0,
            passed=False,
        )
        results.append(pr)
        return results

    # Structure check: same columns
    out_cols = set(output_df.columns)
    base_cols = set(baseline_df.columns)

    if out_cols != base_cols:
        missing = base_cols - out_cols
        extra = out_cols - base_cols
        msg_parts = []
        if missing:
            msg_parts.append(f"missing: {missing}")
        if extra:
            msg_parts.append(f"extra: {extra}")
        pr = PropertyResult(
            name=f"<column-mismatch: {'; '.join(msg_parts)}>",
            max_delta=float("inf"),
            tol_abs=0,
            tol_rel=0,
            baseline_at_max=0,
            passed=False,
        )
        results.append(pr)
        return results

    # Structure check: row count
    if len(output_df) != len(baseline_df):
        pr = PropertyResult(
            name=f"<row-count: output={len(output_df)} baseline={len(baseline_df)}>",
            max_delta=float("inf"),
            tol_abs=0,
            tol_rel=0,
            baseline_at_max=0,
            passed=False,
        )
        results.append(pr)
        return results

    # Check for NaN indicating data misalignment
    delta = np.abs(output_df - baseline_df)
    if delta.isnull().any().any():
        pr = PropertyResult(
            name="<NaN-in-delta: data misalignment>",
            max_delta=float("inf"),
            tol_abs=0,
            tol_rel=0,
            baseline_at_max=0,
            passed=False,
        )
        results.append(pr)
        return results

    # Per-property comparison (skip the time column)
    for col in output_df.columns:
        # Skip time column (typically first column, named "Time" or similar)
        col_lower = col.strip().lower()
        if col_lower in ("time", "t", "time (s)", "time(s)"):
            continue

        # Strip the common JSBSim property prefix for tolerance lookup
        prop_name = col.strip()
        # Try matching with and without common prefixes
        tol_spec = of_spec.get_tolerance(prop_name)
        if tol_spec is None:
            # Try stripping fdm/jsbsim/ prefix
            short_name = strip_jsbsim_prefix(prop_name)
            if short_name != prop_name:
                tol_spec = of_spec.get_tolerance(short_name)
        if tol_spec is None:
            # Fall back to default with the original column name
            tol_spec = of_spec.get_tolerance(prop_name)

        if tol_spec is None:
            # No tolerance at all - create one with None values (will fail)
            tol_spec = ToleranceSpec(prop_name)

        pr = check_property(output_df[col], baseline_df[col], tol_spec)
        pr.name = prop_name  # Use the CSV column name for reporting
        results.append(pr)

    return results


def format_report(test_results, aircraft_name):
    """Format a human-readable validation report."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = []
    lines.append("=" * 66)
    lines.append(f"aceFM Validation Report  --  {aircraft_name}  --  {timestamp}")
    lines.append("=" * 66)
    lines.append("")

    for tr in test_results:
        if tr.run_error:
            lines.append(f"[ERROR] {tr.test_spec.name}  ({tr.run_error})")
        elif tr.passed:
            parts = []
            if tr.num_total:
                parts.append(f"{tr.num_passed}/{tr.num_total} properties")
            if tr.check_results:
                n_chk_pass = sum(1 for cr in tr.check_results if cr.passed)
                parts.append(f"{n_chk_pass}/{len(tr.check_results)} checks")
            lines.append(
                f"[PASS] {tr.test_spec.name}  "
                f"({', '.join(parts) if parts else 'ok'} within tolerance)"
            )
        else:
            parts = []
            if tr.num_failed:
                parts.append(f"{tr.num_failed}/{tr.num_total} properties FAILED")
            chk_fails = [cr for cr in tr.check_results if not cr.passed]
            if chk_fails:
                parts.append(f"{len(chk_fails)}/{len(tr.check_results)} checks FAILED")
            lines.append(
                f"[FAIL] {tr.test_spec.name} "
                f"({'; '.join(parts)})"
            )

    # Print failure details
    any_failures = False
    for tr in test_results:
        if tr.failures:
            if not any_failures:
                lines.append("")
            any_failures = True
            lines.append(f"  FAILURES - {tr.test_spec.name}:")
            lines.append(
                f"  {'Property':<40s} {'MaxDelta':>10s} {'TolAbs':>10s} "
                f"{'Baseline':>10s} {'%Err':>8s}"
            )
            for f in tr.failures:
                tol_abs_str = f"{f.tol_abs:.4g}" if f.tol_abs is not None else "N/A"
                base_str = f"{f.baseline_at_max:.4g}" if f.baseline_at_max is not None else "N/A"
                pct_str = f"{f.pct_err:.1f}%" if f.pct_err is not None else "N/A"
                lines.append(
                    f"  {f.name:<40s} {f.max_delta:>10.4g} {tol_abs_str:>10s} "
                    f"{base_str:>10s} {pct_str:>8s}"
                )
            lines.append("")

    # Summary
    num_passed = sum(1 for tr in test_results if tr.passed)
    num_failed = sum(1 for tr in test_results if not tr.passed)
    num_error = sum(1 for tr in test_results if tr.run_error)
    lines.append(f"Overall: {num_passed} PASSED / {num_failed} FAILED", )
    if num_error:
        lines.append(f"         ({num_error} had runtime errors)")
    lines.append("")

    return "\n".join(lines)


def generate_plots(aircraft_dir, tests, test_results, show=False):
    """Generate comparison plots for each test output file vs its baseline.

    Returns a dict mapping test_spec.name -> list of plot file paths.
    """
    # The first import of matplotlib initialises a backend and may rebuild the
    # font cache — this can take many seconds (occasionally much longer) with
    # no other output, so announce it before it happens.
    print("  Loading plotting library (matplotlib)...")
    import matplotlib
    # Use the non-interactive Agg backend unless plots are shown. The default
    # on this machine is an interactive GUI backend (tkagg) whose first import
    # initialises Tk and rebuilds the font cache — a multi-second silent stall
    # — and renders every figure more slowly. Agg avoids both.
    if not show:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    val_pkg_dir = Path(aircraft_dir) / "autotest" / "validation-package"
    output_dir = Path(aircraft_dir) / "autotest" / "output"

    # Build a lookup from test name to its TestResult for pass/fail info
    result_map = {tr.test_spec.name: tr for tr in test_results}
    plot_paths = {}  # test_name -> [Path, ...]

    # Count the output files that will actually be plotted so progress can be
    # reported as i/N. A plot that hangs then shows exactly which one.
    n_plots = sum(
        1
        for ts in tests
        for of in ts.output_files
        if not (result_map.get(ts.name) and result_map[ts.name].run_error)
        and (Path(aircraft_dir) / of.output_name).exists()
        and (val_pkg_dir / of.baseline_name).exists()
    )
    plot_idx = 0

    for test_spec in tests:
        tr = result_map.get(test_spec.name)
        if tr and tr.run_error:
            continue

        for of_spec in test_spec.output_files:
            output_path = Path(aircraft_dir) / of_spec.output_name
            baseline_path = val_pkg_dir / of_spec.baseline_name

            if not output_path.exists() or not baseline_path.exists():
                continue

            plot_idx += 1
            print(f"  Plotting [{plot_idx}/{n_plots}]: {test_spec.name} "
                  f"({Path(of_spec.baseline_name).stem})...")

            try:
                output_df = pd.read_csv(output_path)
                baseline_df = pd.read_csv(baseline_path)
            except Exception:
                continue

            # Find the time column
            time_col = None
            for col in output_df.columns:
                if col.strip().lower() in ("time", "t", "time (s)", "time(s)"):
                    time_col = col
                    break

            time_out = output_df[time_col] if time_col else output_df.index
            time_base = baseline_df[time_col] if time_col else baseline_df.index

            # Collect data columns common to both (skip time). Output and
            # baseline can diverge when the output directive gains or loses a
            # property after the baseline was promoted; plotting a column the
            # baseline lacks would raise KeyError and abort the whole run, so
            # restrict to the intersection and note any dropped columns.
            base_cols = set(baseline_df.columns)
            data_cols = [c for c in output_df.columns
                         if c != time_col and c in base_cols]
            dropped = [c for c in output_df.columns
                       if c != time_col and c not in base_cols]
            if dropped:
                print(f"  NOTE: {test_spec.name}: {len(dropped)} column(s) "
                      f"not in baseline, omitted from plot: "
                      + ", ".join(strip_jsbsim_prefix(c) for c in dropped))
            if not data_cols:
                continue

            # Build per-property pass/fail lookup from results
            prop_passed = {}
            if tr:
                for pr in tr.property_results:
                    prop_passed[pr.name] = pr.passed

            # Build per-property failed-check times for segmented line colouring.
            # Maps stripped property name -> list of failure sim-times.
            prop_fail_times = {}
            if tr:
                for cr in tr.check_results:
                    if not cr.passed and cr.time is not None:
                        # Match using the raw property and also the stripped
                        # form used by CSV column names.
                        prop_fail_times.setdefault(cr.property, []).append(cr.time)
                        stripped = strip_jsbsim_prefix(cr.property)
                        if stripped != cr.property:
                            prop_fail_times.setdefault(stripped, []).append(cr.time)

            n_props = len(data_cols)
            grid_cols = min(4, n_props)
            grid_rows = (n_props + grid_cols - 1) // grid_cols
            fig, axes = plt.subplots(
                grid_rows, grid_cols,
                figsize=(4 * grid_cols, 3 * grid_rows),
                squeeze=False,
            )
            fig.suptitle(
                f"{test_spec.name}\n{test_spec.description}",
                fontsize=12,
                fontweight="bold",
            )

            for i, col in enumerate(data_cols):
                ax = axes[i // grid_cols, i % grid_cols]
                prop_name = col.strip()

                ax.plot(time_base, baseline_df[col], "b-", label="Baseline", linewidth=1.2)

                # Check if this property has any failed checks — if so, draw
                # the output line in segments: dark-green before the first
                # failure, red from the failure time onward.
                fail_times = prop_fail_times.get(prop_name)
                # Also try without fdm/jsbsim prefix if not found
                if not fail_times:
                    stripped = strip_jsbsim_prefix(prop_name)
                    if stripped != prop_name:
                        fail_times = prop_fail_times.get(stripped)
                if fail_times:
                    t_fail = min(fail_times)  # earliest failure time
                    t_arr = time_out.values if hasattr(time_out, "values") else np.array(time_out)
                    d_arr = output_df[col].values

                    # Segment before failure (inclusive of the boundary sample)
                    mask_before = t_arr <= t_fail
                    mask_after = t_arr >= t_fail
                    if mask_before.any():
                        ax.plot(t_arr[mask_before], d_arr[mask_before],
                                color="#2ca02c", linestyle="--", linewidth=1.0,
                                label="Output")
                    if mask_after.any():
                        ax.plot(t_arr[mask_after], d_arr[mask_after],
                                color="red", linestyle="--", linewidth=1.4,
                                label="Output (FAIL)")

                    # Draw red marker at the failure time(s) and vertical line
                    for ft in sorted(set(fail_times)):
                        ax.axvline(x=ft, color="red", linewidth=0.8, alpha=0.5,
                                   linestyle=":")
                        # Find nearest sample for the marker dot
                        idx = np.argmin(np.abs(t_arr - ft))
                        ax.plot(t_arr[idx], d_arr[idx], "ro", markersize=5,
                                zorder=5)
                else:
                    ax.plot(time_out, output_df[col], "r--", label="Output",
                            linewidth=1.0)

                # Add tolerance band around baseline if abs tolerance defined
                tol_spec = of_spec.get_tolerance(prop_name)
                if tol_spec is None:
                    stripped = strip_jsbsim_prefix(prop_name)
                    if stripped != prop_name:
                        tol_spec = of_spec.get_tolerance(stripped)
                if tol_spec is None:
                    tol_spec = of_spec.get_tolerance(prop_name)

                if tol_spec and tol_spec.tol_abs is not None:
                    ax.fill_between(
                        time_base,
                        baseline_df[col] - tol_spec.tol_abs,
                        baseline_df[col] + tol_spec.tol_abs,
                        alpha=0.15,
                        color="blue",
                        label=f"tolerance={tol_spec.tol_abs:g}",
                    )

                # Colour the property label by combined pass/fail
                # (CSV baseline comparison AND event check results).
                csv_passed = prop_passed.get(prop_name, None)
                has_check_fail = bool(fail_times)  # reuse from line-drawing lookup
                if has_check_fail and csv_passed is False:
                    status_str = " [FAIL]"
                    title_color = "red"
                elif has_check_fail:
                    status_str = " [CHECK FAIL]"
                    title_color = "red"
                elif csv_passed is False:
                    status_str = " [FAIL]"
                    title_color = "red"
                elif csv_passed is True:
                    status_str = " [PASS]"
                    title_color = "green"
                else:
                    status_str = ""
                    title_color = "black"

                short_name = strip_jsbsim_prefix(prop_name)
                ax.set_title(f"{short_name}{status_str}", fontsize=9, color=title_color, loc="left")
                # De-duplicate legend entries from segmented lines
                handles, labels = ax.get_legend_handles_labels()
                seen = set()
                unique_h, unique_l = [], []
                for h, l in zip(handles, labels):
                    if l not in seen:
                        seen.add(l)
                        unique_h.append(h)
                        unique_l.append(l)
                ax.legend(unique_h, unique_l, fontsize=7, loc="upper right")
                ax.grid(True, alpha=0.3)
                ax.tick_params(labelsize=7)

            # Label bottom row and hide unused cells
            for c in range(grid_cols):
                axes[-1, c].set_xlabel("Time (s)", fontsize=8)
            for i in range(n_props, grid_rows * grid_cols):
                axes[i // grid_cols, i % grid_cols].set_visible(False)
            fig.tight_layout(rect=[0, 0, 1, 0.96])

            # Save as PNG
            plot_stem = Path(of_spec.baseline_name).stem
            plot_path = output_dir / f"{plot_stem}_comparison.png"
            fig.savefig(plot_path, dpi=150)
            if not show:
                plt.close(fig)
            plot_paths.setdefault(test_spec.name, []).append(plot_path)
            print(f"  Plot saved: {plot_path}")

    if show:
        plt.show()

    return plot_paths


def _test_group(test_spec):
    """Extract the group (directory) from a test's script path.

    e.g. script="apu-intake-shutter/apu-intake-cold-dark.xml"
         -> "apu-intake-shutter"
         script="level_accel.xml"
         -> "level_accel"  (falls back to test name)
    """
    script = test_spec.script
    if "/" in script:
        return script.rsplit("/", 1)[0]
    return test_spec.name


_UNIT_SUFFIXES = [
    ("-kts", "kts"),
    ("-fps", "ft/s"),
    ("-deg", "deg"),
    ("-ft", "ft"),
    ("-lbs", "lbs"),
    ("-norm", "norm"),
    ("-percent", "%"),
    ("-pct", "%"),
    ("-rad_sec", "rad/s"),
    ("-psf", "lbf/ft\u00b2"),
    ("-c", "\u00b0C"),
    ("-R", "\u00b0R"),
    ("slugs_ft3", "slug/ft\u00b3"),
]


def _infer_unit(prop_name):
    """Extract unit from a JSBSim property name suffix."""
    for suffix, unit in _UNIT_SUFFIXES:
        if prop_name.endswith(suffix):
            return unit
    return ""


_EVENT_RE = re.compile(r"^(.+?) \(Event (\d+)\) executed at time:\s+([\d.]+)")
_TIMER_PREFIXES = ("[Begin]", "[JSBSIM setup]", "[Load model]", "[Start]", "[End]")

# Regexes for CHECK/EVENT result lines from JSBSim <check> elements
_CHECK_RE = re.compile(
    r"^\s+CHECK (PASS|FAIL): (.+?) = ([\d.eE+\-]+) expected ([\d.eE+\-]+) tol ([\d.eE+\-]+)"
)
_CHECK_MSG_RE = re.compile(
    r"^\s+CHECK (PASS|FAIL): (.+?) = ([\d.eE+\-]+) expected ([\d.eE+\-]+) tol ([\d.eE+\-]+)\s+: (.+)"
)
_EVENT_RESULT_RE = re.compile(
    r"^\s+EVENT (PASS|FAIL): (.+?) \((\d+)"
)

# Regex for State Report data lines, e.g.:
#     Local: 51.457696, -0.300000, 3.000016 (geodetic lat, lon, alt ASL in deg and ft)
_IC_DATA_RE = re.compile(
    r"^\s{4}(\w[\w/]*)\s*:\s*"          # frame name (e.g. "Local", "ECI", "Body")
    r"([\d.eE+\-,\s]+?)"                # comma-separated numeric values
    r"\s*\(([^)]+)\)\s*$"               # parenthesised description
)


def _parse_state_report(block):
    """Parse a State Report text block into a list of row dicts.

    Each row has: section, frame, values (str), description.
    Returns (sim_time_str, rows).
    """
    lines = block.splitlines()
    rows = []
    sim_time = ""
    current_section = ""

    for line in lines:
        # Header: "State Report at sim time: 0.000000 seconds"
        tm = re.search(r"State Report at sim time:\s*([\d.]+)\s*seconds", line)
        if tm:
            sim_time = tm.group(1)
            continue

        # Section header (2-space indent): "  Position", "  Orientation",
        # "  Body Rates (relative to given frame, expressed in body frame)"
        sm = re.match(r"^  (\S.+?)\s*$", line)
        # Only accept as section header if it's NOT a data line (no colon
        # followed by numbers)
        if sm and _IC_DATA_RE.match(line):
            sm = None
        if sm:
            # Strip parenthesised qualifiers, e.g.
            # "Body Rates (relative to given frame, ...)" -> "Body Rates"
            current_section = re.sub(r"\s*\(.*\)\s*$", "", sm.group(1))
            continue

        # Data line (4-space indent): "    Local: 51.45, -0.30, 3.00 (desc)"
        dm = _IC_DATA_RE.match(line)
        if dm:
            frame = dm.group(1)
            if frame == "ECEF":
                continue
            raw_vals = [v.strip() for v in dm.group(2).split(",")]
            formatted = []
            for v in raw_vals:
                try:
                    formatted.append(f"{float(v):.2f}")
                except ValueError:
                    formatted.append(v)
            values = ", ".join(formatted)
            desc = dm.group(3)
            rows.append({
                "section": current_section,
                "frame": frame,
                "values": values,
                "description": desc,
            })

    return sim_time, rows


def parse_testplane_output(stdout):
    """Parse TestPlane.exe stdout into structured sections.

    Returns a dict with:
        initial_conditions: list of row dicts (section, frame, values, description)
        ic_sim_time: str — sim time from the State Report header
        events: list of dicts — each with name, index, time, properties
    """
    lines = stdout.splitlines()
    ic_rows = []
    ic_sim_time = ""
    events = []

    # --- Extract initial conditions (State Report at sim time: ...) ---
    # The block starts with "State Report at sim time:" and contains
    # indented subsections (Position, Orientation, Velocity, Body Rates).
    # It ends at the first non-blank, non-indented line after content has
    # been seen, or at a known boundary like "---- JSBSim Execution".
    ic_start = None
    ic_end = None
    for i, line in enumerate(lines):
        if "State Report at sim time:" in line:
            ic_start = i
        elif ic_start is not None:
            if line.startswith("---- JSBSim Execution"):
                ic_end = i
                break
            # End when we hit a non-blank line that isn't indented
            # (the State Report body is entirely indented)
            if line.strip() and not line.startswith(" "):
                ic_end = i
                break

    if ic_start is not None:
        end = ic_end if ic_end is not None else len(lines)
        block = "\n".join(lines[ic_start:end]).rstrip()
        ic_sim_time, ic_rows = _parse_state_report(block)

    # --- Extract events and CHECK results (in a single pass) ---
    # CHECK lines appear within event blocks so we track the current event
    # time to associate each check with its simulation time.
    checks = []
    event_results = []
    i = 0
    while i < len(lines):
        m = _EVENT_RE.match(lines[i])
        if m:
            event = {
                "name": m.group(1),
                "index": int(m.group(2)),
                "time": float(m.group(3)),
                "properties": OrderedDict(),
            }
            current_event_time = float(m.group(3))
            i += 1
            # Collect indented property / CHECK / EVENT-result lines
            while i < len(lines):
                pline = lines[i]
                # CHECK lines (4-space indent, "CHECK PASS/FAIL:")
                cm = _CHECK_MSG_RE.match(pline)
                if not cm:
                    cm_plain = _CHECK_RE.match(pline)
                else:
                    cm_plain = None
                if cm:
                    checks.append({
                        "passed": cm.group(1) == "PASS",
                        "property": cm.group(2),
                        "actual": float(cm.group(3)),
                        "expected": float(cm.group(4)),
                        "tolerance": float(cm.group(5)),
                        "message": cm.group(6),
                        "time": current_event_time,
                    })
                    i += 1
                    continue
                if cm_plain:
                    checks.append({
                        "passed": cm_plain.group(1) == "PASS",
                        "property": cm_plain.group(2),
                        "actual": float(cm_plain.group(3)),
                        "expected": float(cm_plain.group(4)),
                        "tolerance": float(cm_plain.group(5)),
                        "message": "",
                        "time": current_event_time,
                    })
                    i += 1
                    continue
                # EVENT result lines (2-space indent)
                em = _EVENT_RESULT_RE.match(pline)
                if em:
                    event_results.append({
                        "passed": em.group(1) == "PASS",
                        "name": em.group(2),
                        "count": int(em.group(3)),
                        "time": current_event_time,
                    })
                    i += 1
                    continue
                # Property lines (4-space indent with " = ")
                if pline.startswith("    ") and " = " in pline:
                    key, _, val = pline.strip().partition(" = ")
                    event["properties"][key] = val
                    i += 1
                else:
                    break
            events.append(event)
        else:
            # Also catch CHECK/EVENT lines outside event blocks (shouldn't
            # normally happen, but be robust)
            cm = _CHECK_MSG_RE.match(lines[i])
            if cm:
                checks.append({
                    "passed": cm.group(1) == "PASS",
                    "property": cm.group(2),
                    "actual": float(cm.group(3)),
                    "expected": float(cm.group(4)),
                    "tolerance": float(cm.group(5)),
                    "message": cm.group(6),
                    "time": None,
                })
                i += 1
                continue
            cm = _CHECK_RE.match(lines[i])
            if cm:
                checks.append({
                    "passed": cm.group(1) == "PASS",
                    "property": cm.group(2),
                    "actual": float(cm.group(3)),
                    "expected": float(cm.group(4)),
                    "tolerance": float(cm.group(5)),
                    "message": "",
                    "time": None,
                })
                i += 1
                continue
            em = _EVENT_RESULT_RE.match(lines[i])
            if em:
                event_results.append({
                    "passed": em.group(1) == "PASS",
                    "name": em.group(2),
                    "count": int(em.group(3)),
                    "time": None,
                })
            i += 1

    return {
        "initial_conditions": ic_rows,
        "ic_sim_time": ic_sim_time,
        "events": events,
        "checks": checks,
        "event_results": event_results,
    }




_NS = "urn:aviastorm:autotest-results:1.0"


def _xslt_href(xml_path, xslt_name):
    """Ensure the XSLT is in the same directory as the XML and return its name.

    Copies the stylesheet from schema/ into the output directory so browsers
    can resolve it (same-origin policy blocks cross-directory file:// refs).
    Returns the bare filename for use in <?xml-stylesheet?>.
    """
    schema_dir = Path(__file__).resolve().parent / "schema"
    xslt_src = schema_dir / xslt_name
    if not xslt_src.exists():
        return None
    xslt_dest = xml_path.parent / xslt_name
    # Copy if missing or source is newer
    if not xslt_dest.exists() or xslt_src.stat().st_mtime > xslt_dest.stat().st_mtime:
        shutil.copy2(xslt_src, xslt_dest)
    return xslt_name


def _pretty_xml(root_elem, stylesheet=None):
    """Serialize an ElementTree root to pretty-printed XML bytes (UTF-8).

    If stylesheet is provided, inserts an <?xml-stylesheet?> PI after the
    XML declaration so browsers can render via XSLT.
    """
    rough = ET.tostring(root_elem, encoding="unicode")
    parsed = minidom.parseString(rough)
    if stylesheet:
        pi = parsed.createProcessingInstruction(
            "xml-stylesheet", f'type="text/xsl" href="{stylesheet}"')
        parsed.insertBefore(pi, parsed.documentElement)
    return parsed.toprettyxml(indent="  ", encoding="utf-8")


def _se(parent, tag, text=None, **attribs):
    """Create a sub-element with optional text and attributes."""
    elem = ET.SubElement(parent, tag, **attribs)
    if text is not None:
        elem.text = str(text)
    return elem


def generate_xml_report(aircraft_dir, tests, test_results, plot_paths,
                        group_metadata=None, suite_metadata=None):
    """Generate one XML report per test group in output/.

    Returns a dict mapping group_name -> Path to generated XML file.
    """
    output_dir = Path(aircraft_dir) / "autotest" / "output"
    result_map = {tr.test_spec.name: tr for tr in test_results}

    # Group tests by script directory
    groups = OrderedDict()
    for test_spec in tests:
        group = _test_group(test_spec)
        groups.setdefault(group, []).append(test_spec)

    # Resolve group metadata
    group_defs = {}
    if group_metadata:
        group_defs = group_metadata
    else:
        for group in groups:
            td = _parse_test_definition(aircraft_dir, group)
            if td:
                group_defs[group] = td

    written = {}
    for group, group_tests in groups.items():
        root = ET.Element("autotest-report", xmlns=_NS)

        # -- metadata --
        meta_elem = _se(root, "metadata")
        td = group_defs.get(group, {})

        doc_number = td.get("doc_number", "")
        if doc_number and not doc_number.endswith("-AT"):
            doc_number += "-AT"
        doc_title = (group.replace("-", " ").replace("_", " ").title()
                     + " - Autotest Results")
        doc_date = datetime.now().strftime("%d %b %Y").upper()

        _se(meta_elem, "document-number", doc_number)
        _se(meta_elem, "title", doc_title)
        _se(meta_elem, "date", doc_date)
        _se(meta_elem, "aircraft", td.get("aircraft", ""))
        _se(meta_elem, "system", td.get("system", ""))
        _se(meta_elem, "ata-chapter", td.get("ata_chapter", ""))
        _se(meta_elem, "reference", td.get("reference", ""))
        _se(meta_elem, "revision", td.get("revision", "A (Initial Release)"))
        _se(meta_elem, "prepared-by", td.get("author", ""))
        _se(meta_elem, "classification",
            td.get("classification", "UNCLASSIFIED -- For Simulation Use Only"))
        sm = suite_metadata or {}
        if sm.get("company"):
            _se(meta_elem, "company", sm["company"])
        if sm.get("logo"):
            _se(meta_elem, "logo", sm["logo"])

        # -- group summary --
        group_results = [result_map[t.name] for t in group_tests
                         if t.name in result_map]
        n_pass = sum(1 for tr in group_results if tr.passed)
        n_fail = sum(1 for tr in group_results
                     if not tr.passed and not tr.run_error)
        n_error = sum(1 for tr in group_results if tr.run_error)
        n_total = len(group_results)
        _se(root, "summary",
            passed=str(n_pass), failed=str(n_fail),
            errors=str(n_error), total=str(n_total))

        # -- per-test sections --
        for test_spec in group_tests:
            tr = result_map.get(test_spec.name)
            test_elem = _se(root, "test", name=test_spec.name)
            _se(test_elem, "description", test_spec.description or "")

            if tr is None:
                _se(test_elem, "result", status="not-run")
                continue

            if tr.run_error:
                res = _se(test_elem, "result", status="error")
                _se(res, "message", tr.run_error)
                continue

            status = "pass" if tr.passed else "fail"
            result_attrs = {
                "properties-passed": str(tr.num_passed),
                "properties-total": str(tr.num_total),
            }
            if tr.check_results:
                n_chk_pass = sum(1 for cr in tr.check_results if cr.passed)
                result_attrs["checks-passed"] = str(n_chk_pass)
                result_attrs["checks-total"] = str(len(tr.check_results))
            _se(test_elem, "result", status=status, **result_attrs)

            # property-results
            if tr.property_results:
                pr_container = _se(test_elem, "property-results")
                for pr in tr.property_results:
                    attrs = {
                        "name": strip_jsbsim_prefix(pr.name),
                        "max-delta": f"{pr.max_delta:.6g}",
                        "passed": str(pr.passed).lower(),
                    }
                    if pr.tol_abs is not None:
                        attrs["tol-abs"] = f"{pr.tol_abs:g}"
                    if pr.tol_rel is not None:
                        attrs["tol-rel"] = f"{pr.tol_rel:g}"
                    if pr.baseline_at_max is not None:
                        attrs["baseline-at-max"] = f"{pr.baseline_at_max:.6g}"
                    _se(pr_container, "property", **attrs)

            # check-results
            if tr.check_results:
                cr_container = _se(test_elem, "check-results")
                for cr in tr.check_results:
                    attrs = {
                        "property": strip_jsbsim_prefix(cr.property),
                        "expected": f"{cr.expected:g}",
                        "actual": f"{cr.actual:g}",
                        "tolerance": f"{cr.tolerance:g}",
                        "passed": str(cr.passed).lower(),
                    }
                    if cr.time is not None:
                        attrs["time"] = f"{cr.time:g}"
                    if cr.message:
                        attrs["message"] = cr.message
                    _se(cr_container, "check", **attrs)

            # initial-conditions and events from parsed stdout
            if tr.stdout:
                parsed = parse_testplane_output(tr.stdout)

                if parsed["initial_conditions"]:
                    ic_container = _se(test_elem, "initial-conditions")
                    for row in parsed["initial_conditions"]:
                        _se(ic_container, "condition",
                            section=row["section"], frame=row["frame"],
                            values=row["values"],
                            description=row["description"])

                if parsed["events"]:
                    ev_container = _se(test_elem, "events")
                    for ev in parsed["events"]:
                        ev_elem = _se(ev_container, "event",
                                      name=ev["name"],
                                      time=f"{ev['time']:.3f}")
                        for k, v in ev["properties"].items():
                            _se(ev_elem, "property", name=k, value=v)

            # plots
            test_plots = plot_paths.get(test_spec.name, [])
            if test_plots:
                plots_container = _se(test_elem, "plots")
                for pp in test_plots:
                    _se(plots_container, "plot", file=pp.name)

        # Write XML (with browser-viewable XSLT reference)
        xml_path = output_dir / f"{group}.xml"
        xslt_href = _xslt_href(xml_path, "autotest-report-html.xslt")
        xml_path.write_bytes(_pretty_xml(root, stylesheet=xslt_href))
        written[group] = xml_path
        print(f"  XML report saved: {xml_path}")

    return written


def generate_xml_summary(aircraft_dir, aircraft_name, tests, test_results,
                         xml_reports, group_metadata=None,
                         suite_metadata=None):
    """Generate a summary XML file linking to per-group XML reports."""
    output_dir = Path(aircraft_dir) / "autotest" / "output"
    result_map = {tr.test_spec.name: tr for tr in test_results}

    root = ET.Element("autotest-summary", xmlns=_NS)

    # metadata
    meta_elem = _se(root, "metadata")
    _se(meta_elem, "aircraft", aircraft_name)
    _se(meta_elem, "date", datetime.now().strftime("%d %b %Y").upper())
    sm = suite_metadata or {}
    if sm.get("company"):
        _se(meta_elem, "company", sm["company"])
    if sm.get("logo"):
        _se(meta_elem, "logo", sm["logo"])

    # overall summary
    n_pass = sum(1 for tr in test_results if tr.passed)
    n_fail = sum(1 for tr in test_results
                 if not tr.passed and not tr.run_error)
    n_error = sum(1 for tr in test_results if tr.run_error)
    n_total = len(test_results)
    _se(root, "summary",
        passed=str(n_pass), failed=str(n_fail),
        errors=str(n_error), total=str(n_total))

    # Resolve group metadata
    group_defs = {}
    if group_metadata:
        group_defs = group_metadata
    else:
        groups_set = OrderedDict()
        for t in tests:
            groups_set.setdefault(_test_group(t), [])
        for group in groups_set:
            td = _parse_test_definition(aircraft_dir, group)
            if td:
                group_defs[group] = td

    # groups
    groups = OrderedDict()
    for test_spec in tests:
        group = _test_group(test_spec)
        groups.setdefault(group, []).append(test_spec)

    groups_elem = _se(root, "groups")
    for group, group_tests in groups.items():
        g_results = [result_map[t.name] for t in group_tests
                     if t.name in result_map]
        g_pass = sum(1 for tr in g_results if tr.passed)
        g_fail = sum(1 for tr in g_results
                     if not tr.passed and not tr.run_error)
        g_err = sum(1 for tr in g_results if tr.run_error)

        td = group_defs.get(group, {})
        doc_number = td.get("doc_number", "")
        report_file = f"{group}.xml"

        _se(groups_elem, "group",
            name=group,
            **{"document-number": doc_number},
            report=report_file,
            tests=str(len(g_results)),
            passed=str(g_pass), failed=str(g_fail), errors=str(g_err))

    # all tests
    tests_elem = _se(root, "tests")
    for test_spec in tests:
        tr = result_map.get(test_spec.name)
        group = _test_group(test_spec)
        if tr is None:
            _se(tests_elem, "test", name=test_spec.name, group=group,
                status="not-run", properties="0/0")
        elif tr.run_error:
            _se(tests_elem, "test", name=test_spec.name, group=group,
                status="error", properties="0/0")
        else:
            status = "pass" if tr.passed else "fail"
            _se(tests_elem, "test", name=test_spec.name, group=group,
                status=status,
                properties=f"{tr.num_passed}/{tr.num_total}")

    xml_path = output_dir / "summary.xml"
    xslt_href = _xslt_href(xml_path, "autotest-summary-html.xslt")
    xml_path.write_bytes(_pretty_xml(root, stylesheet=xslt_href))
    print(f"  XML summary saved: {xml_path}")
    return xml_path


_TEST_DEF_FIELDS = ("system", "ata_chapter", "reference", "author", "doc_number",
                     "documentation", "aircraft", "revision", "classification")


def _parse_test_definition(aircraft_dir, group):
    """Parse an optional test-definition.xml from a test group's script directory.

    Returns a dict of metadata fields, or None if the file doesn't exist.
    The doc_number is rewritten to start with TR- (replacing any leading TN-).
    """
    td_path = Path(aircraft_dir) / "autotest" / "scripts" / group / "test-definition.xml"
    if not td_path.exists():
        # Also check one level up in case group is the scripts dir itself
        td_path = Path(aircraft_dir) / "autotest" / group / "test-definition.xml"
    if not td_path.exists():
        return None

    try:
        tree = ET.parse(td_path)
    except ET.ParseError:
        return None

    root = tree.getroot()
    meta = {}
    for field in _TEST_DEF_FIELDS:
        elem = root.find(field)
        if elem is not None and elem.text:
            meta[field] = elem.text.strip()

    # Rewrite doc_number prefix from TN- to TR-
    if "doc_number" in meta:
        dn = meta["doc_number"]
        if dn.startswith("TN-"):
            meta["doc_number"] = "TR-" + dn[3:]
        elif dn.startswith("TN_"):
            meta["doc_number"] = "TR-" + dn[3:]

    return meta if meta else None




def _precision_from_tolerance(tol_spec):
    """Derive a formatting precision from a ToleranceSpec.

    Returns (mode, n) where mode is 'decimals' or 'sigfigs', or None if the
    spec has no usable tolerance. One extra digit of margin is added so
    round-trip noise stays well below the tolerance.
    """
    if tol_spec is None:
        return None
    tol_abs = tol_spec.tol_abs
    tol_rel = tol_spec.tol_rel
    if tol_abs is not None and tol_abs > 0:
        decimals = max(0, int(math.ceil(-math.log10(tol_abs))) + 1)
        return ("decimals", decimals)
    if tol_rel is not None and tol_rel > 0:
        sigfigs = max(1, int(math.ceil(-math.log10(tol_rel))) + 1)
        return ("sigfigs", sigfigs)
    return None


def _format_quantised(value, prec):
    """Format a float to the given precision, trimming trailing zeros."""
    if prec is None or pd.isna(value):
        return value
    mode, n = prec
    if mode == "decimals":
        return np.format_float_positional(
            float(value), precision=n, unique=False, fractional=True, trim="-"
        )
    return np.format_float_positional(
        float(value), precision=n, unique=False, fractional=False, trim="-"
    )


def _quantise_csv_for_baseline(output_path, dest_path, of_spec):
    """Write output_path to dest_path, rounding each column to its tolerance.

    Columns with no tolerance (including Time) are written as pandas would
    normally render them.
    """
    df = pd.read_csv(output_path)

    col_prec = {}
    for col in df.columns:
        col_clean = col.strip()
        if col_clean.lower() in ("time", "t", "time (s)", "time(s)"):
            continue
        tol = of_spec.get_tolerance(col_clean)
        if tol is None:
            short = strip_jsbsim_prefix(col_clean)
            if short != col_clean:
                tol = of_spec.get_tolerance(short)
        prec = _precision_from_tolerance(tol)
        if prec is not None:
            col_prec[col] = prec

    if col_prec:
        out_df = df.copy()
        for col, prec in col_prec.items():
            out_df[col] = df[col].map(lambda v, p=prec: _format_quantised(v, p))
    else:
        out_df = df

    out_df.to_csv(dest_path, index=False)


def promote_baselines(aircraft_dir, tests, test_filter=None):
    """
    Copy output CSVs to validation-package as new baselines.

    Values are quantised to the precision implied by each property's
    tolerance so that insignificant floating-point noise between runs does
    not show up as a git diff on promoted baselines.

    If test_filter is provided, only promote that test.
    """
    val_pkg_dir = Path(aircraft_dir) / "autotest" / "validation-package"
    val_pkg_dir.mkdir(parents=True, exist_ok=True)

    promoted = []
    for test in tests:
        if test_filter and test.name != test_filter:
            continue
        for of_spec in test.output_files:
            output_path = Path(aircraft_dir) / of_spec.output_name
            baseline_dest = val_pkg_dir / of_spec.baseline_name
            if not output_path.exists():
                print(f"  WARNING: Output not found: {output_path}")
                continue
            try:
                _quantise_csv_for_baseline(output_path, baseline_dest, of_spec)
            except Exception as e:
                print(f"  WARNING: quantise failed for {of_spec.output_name} ({e}); copying raw")
                shutil.copy2(output_path, baseline_dest)
            promoted.append((test.name, of_spec.output_name, of_spec.baseline_name))
            print(f"  Promoted: {of_spec.output_name} -> validation-package/{of_spec.baseline_name}")

    return promoted


def main():
    # Stream progress line-by-line even when stdout is redirected or piped
    # (e.g. run from a .bat with output captured, or from an IDE). Without
    # this, Python block-buffers non-tty stdout and the per-test progress is
    # withheld in ~8KB chunks, so a run that is working looks frozen.
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(line_buffering=True)
        except (AttributeError, ValueError):
            pass

    parser = argparse.ArgumentParser(
        description="aceFM QTG-Style Validation Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--jsbsim",
        default=None,
        help="Path to JSBSim.exe (if omitted: JSBSIM_EXE, alongside "
             "--testplane, the local build tree, then PATH)",
    )
    parser.add_argument(
        "--testplane",
        default=None,
        help="Path to TestPlane.exe; used only as a hint for locating "
             "JSBSim.exe in the same directory",
    )
    parser.add_argument(
        "--mode",
        choices=("dcs", "fg", "bridge"),
        default="dcs",
        help="Path convention: dcs (EFM layout), fg (FlightGear layout), or "
             "bridge (dcs with Propagate and GroundReactions disabled)",
    )
    parser.add_argument(
        "--aircraft",
        required=True,
        help="Path to aircraft mod directory",
    )
    parser.add_argument(
        "--promote",
        action="store_true",
        help="Promote output CSVs to baseline (run tests first, then copy)",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display plots interactively (requires matplotlib)",
    )
    parser.add_argument(
        "--fg",
        action="store_true",
        help="Use test-fg mode (FlightGear atmosphere) instead of test",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="count",
        default=0,
        help="Increase verbosity (repeatable). -v shows the JSBSim command "
             "line for each test; -vv adds the working directory, return "
             "code, stderr and resolved output/baseline paths; -vvv adds "
             "the full JSBSim stdout.",
    )
    parser.add_argument(
        "test_name",
        nargs="?",
        default=None,
        help="Run only this test (by name attribute in tolerances.xml)",
    )

    args = parser.parse_args()

    jsbsim_exe = resolve_jsbsim_exe(args)
    mode = "fg" if args.fg else args.mode
    aircraft_dir = Path(args.aircraft).resolve()

    if not aircraft_dir.is_dir():
        print(f"ERROR: Aircraft directory not found at {aircraft_dir}")
        sys.exit(2)
    print(f"Using JSBSim: {jsbsim_exe}  (mode: {mode})")

    tolerances_path = aircraft_dir / "autotest" / "validation-package" / "tolerances.xml"
    autotest_xml_path = aircraft_dir / "autotest" / "autotest.xml"

    group_metadata = None
    suite_metadata = {}
    if not autotest_xml_path.exists():
        print(f"ERROR: autotest.xml not found at {autotest_xml_path}")
        sys.exit(2)
    if not tolerances_path.exists():
        print(f"ERROR: tolerances.xml not found at {tolerances_path}")
        sys.exit(2)
    print("Parsing test configuration and tolerances...")
    _, tests, group_metadata, suite_metadata = parse_autotest_config(autotest_xml_path, tolerances_path)

    if not tests:
        print("No tests defined")
        sys.exit(0)

    # Filter to matching tests if requested (exact match or prefix match)
    if args.test_name:
        # Exact match first
        exact = [t for t in tests if t.name == args.test_name]
        if exact:
            tests = exact
        else:
            # Prefix match on name or script directory:
            # "apu" matches "apu-intake-cold-dark" by name,
            # or tests whose script is in "apu-intake-shutter/" by directory
            prefix = args.test_name
            tests = [
                t for t in tests
                if t.name.startswith(prefix) or _test_group(t).startswith(prefix)
            ]
        if not tests:
            print(f"ERROR: No test matching '{args.test_name}' found in tolerances.xml")
            sys.exit(2)
        if len(tests) > 1:
            print(f"Matched {len(tests)} tests: {', '.join(t.name for t in tests)}")

    aircraft_name = aircraft_dir.name

    # Ensure output directory exists
    output_dir = aircraft_dir / "autotest" / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    # Run tests
    print(f"Running {len(tests)} test(s) for {aircraft_name}...")
    print()

    n_tests = len(tests)
    test_results = []
    for idx, test_spec in enumerate(tests, 1):
        print(f"  [{idx}/{n_tests}] Running: {test_spec.name} "
              f"({test_spec.script}) -- invoking JSBSim (timeout 300s)...")
        script_path = "autotest/" + test_spec.script
        returncode, stdout, stderr = run_test(
            jsbsim_exe, aircraft_dir, script_path, mode=mode,
            verbose=args.verbose,
        )

        result = TestResult(test_spec)
        result.stdout = stdout
        result.run_return_code = returncode

        if args.verbose >= 2:
            print(f"    return code: {returncode}")
            if stderr.strip():
                print("    stderr:")
                for line in stderr.rstrip().splitlines():
                    print(f"      {line}")
        if args.verbose >= 3:
            print("    stdout:")
            for line in stdout.rstrip().splitlines():
                print(f"      {line}")

        if returncode < 0:
            result.run_error = f"TestPlane.exe failed: {stderr.strip()[:200]}"
            test_results.append(result)
            print(f"    ERROR: {result.run_error}")
            continue

        # Parse CHECK results from stdout
        parsed_output = parse_testplane_output(stdout)
        for chk in parsed_output.get("checks", []):
            result.check_results.append(CheckResult(
                property=chk["property"],
                actual=chk["actual"],
                expected=chk["expected"],
                tolerance=chk["tolerance"],
                passed=chk["passed"],
                message=chk.get("message", ""),
                time=chk.get("time"),
            ))

        if returncode == 1:
            # Check failures detected by JSBSim
            n_chk_fail = sum(1 for cr in result.check_results if not cr.passed)
            if n_chk_fail:
                print(f"    CHECK FAILURES: {n_chk_fail} check(s) failed")

        if args.promote:
            # In promote mode, skip comparison - just copy outputs
            test_results.append(result)
            continue

        # Compare each output file
        val_pkg_dir = aircraft_dir / "autotest" / "validation-package"
        for of_spec in test_spec.output_files:
            output_path = aircraft_dir / of_spec.output_name
            baseline_path = val_pkg_dir / of_spec.baseline_name

            if args.verbose >= 2:
                print(f"    output:   {output_path}")
                print(f"    baseline: {baseline_path}")

            if not output_path.exists():
                result.run_error = f"Output file not found: {output_path}"
                print(f"    ERROR: {result.run_error}")
                break

            if not baseline_path.exists():
                result.run_error = (
                    f"Baseline not found: {baseline_path} "
                    f"(use --promote to establish baselines)"
                )
                print(f"    ERROR: {result.run_error}")
                break

            prop_results = compare_csv(output_path, baseline_path, of_spec)
            result.property_results.extend(prop_results)

        test_results.append(result)

    # Handle --promote
    if args.promote:
        print()
        print("Promoting outputs to baselines...")
        promoted = promote_baselines(aircraft_dir, tests)
        if promoted:
            print(f"\n{len(promoted)} file(s) promoted to validation-package/")
        else:
            print("\nNo files promoted (outputs may not exist yet)")
        sys.exit(0)

    # Generate comparison plots and XML reports (skip if no baselines)
    has_baselines = any(not tr.run_error for tr in test_results)
    if has_baselines:
        print()
        print("Generating comparison plots...")
        plot_paths = generate_plots(aircraft_dir, tests, test_results, show=args.show)
        print()
        print("Generating XML reports...")
        xml_reports = generate_xml_report(
            aircraft_dir, tests, test_results, plot_paths, group_metadata,
            suite_metadata
        )
    else:
        plot_paths = {}
        xml_reports = {}

    if not args.test_name:
        if xml_reports:
            print()
            print("Generating XML summary...")
            generate_xml_summary(
                aircraft_dir, aircraft_name, tests, test_results,
                xml_reports, group_metadata, suite_metadata
            )

    # Generate report
    print()
    print("Generating text report...")
    report = format_report(test_results, aircraft_name)
    print()
    print(report)

    # Write report to file
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    report_path = output_dir / f"validation_report_{timestamp}.txt"
    report_path.write_text(report)
    print(f"Report written to: {report_path}")

    # Exit code
    all_passed = all(tr.passed for tr in test_results)
    any_error = any(tr.run_error for tr in test_results)

    if any_error:
        sys.exit(2)
    elif all_passed:
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
