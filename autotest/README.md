# acEFM autotest

How the QTG-style validation runner in this directory works and how to run it.

## What This Is

QTG (Qualification Test Guide)-style regression testing framework for JSBSim aircraft models built for acEFM. Compares simulation output CSVs against known-good baselines using configurable tolerances, generates reports in text/XML/PDF formats with comparison plots.

## Running Tests

```bash
# Run all tests for an aircraft
python run_validation.py --aircraft path/to/AircraftMod

# Run a single test by name
python run_validation.py --aircraft path/to/AircraftMod test_name

# Promote output CSVs to become new baselines
python run_validation.py --aircraft path/to/AircraftMod --promote test_name

# Use the FlightGear path conventions and atmosphere instead of the DCS ones
python run_validation.py --aircraft path/to/AircraftMod --fg-atmosphere

# Point at a specific JSBSim.exe (otherwise: JSBSIM_EXE, next to --testplane,
# the aceFM CMake build tree, then PATH)
python run_validation.py --aircraft path/to/AircraftMod --jsbsim path/to/JSBSim.exe

# Generate PDF reports (requires Apache FOP on PATH)
python generate_pdf_reports.py --aircraft path/to/AircraftMod
python generate_pdf_reports.py --aircraft path/to/AircraftMod --summary
```

Dependencies: `pandas`, `numpy`, `matplotlib`. PDF generation requires Apache FOP.

## Configuration

`autotest/autotest.xml` defines test groups, scripts, and output files.
`validation-package/tolerances.xml` defines tolerances only.
Both files are required.

### Tolerance Lookup Order (lowest → highest priority)

1. Global `<default>` tolerance
2. Global pattern-matched tolerance (fnmatch glob, e.g. `*velocities*`)
3. Per-test `<default>` tolerance
4. Per-test exact-match property tolerance

A property **passes** if `max_delta <= tol_abs` **OR** `max_delta / baseline_magnitude <= tol_rel` (either condition sufficient).

## Architecture

### run_validation.py (~1,750 lines)

Core data flow: **parse config → run JSBSim.exe → parse stdout → compare CSVs → generate reports**

Key classes:
- `ToleranceSpec` — single property's abs/rel tolerance
- `GlobalTolerances` — pattern-matched tolerance rules
- `OutputFileSpec` — one output CSV vs its baseline
- `TestSpec` — full test definition (script, output files, tolerances)
- `PropertyResult` / `CheckResult` / `TestResult` — comparison results

Key functions:
- `parse_autotest_config()` / `parse_tolerances()` — config parsing
- `run_test()` — executes TestPlane.exe via subprocess (300s timeout)
- `parse_testplane_output()` — regex parsing of stdout for initial conditions, events, and JSBSim `<check>` results
- `check_property()` / `compare_csv()` — CSV column comparison against baselines
- `generate_plots()` — matplotlib comparison plots (baseline vs output with tolerance bands)
- `generate_xml_report()` / `generate_xml_summary()` — XML reports matching `schema/autotest-results.xsd`
- `format_report()` — human-readable text report
- `promote_baselines()` — copies output CSVs to validation-package/

### generate_pdf_reports.py

Invokes Apache FOP with XSLT stylesheets from `schema/` to transform XML reports into A4 PDF documents with aviation-style formatting.

### schema/

- `autotest-results.xsd` — XML schema (namespace `urn:aviastorm:autotest-results:1.0`) for report XML
- `autotest-report-html.xslt` / `autotest-report-fo.xslt` — per-group report transforms (HTML / XSL-FO)
- `autotest-summary-html.xslt` / `autotest-summary-fo.xslt` — suite summary transforms

XML reports include `<?xml-stylesheet?>` processing instructions so they render directly in browsers via the HTML XSLT.

## JSBSim.exe Invocation

Each test is a run of the standalone JSBSim script runner from the aircraft mod directory:

```
JSBSim.exe --script=autotest/<script-path> --root=. --aircraft-path=EFM
           --engine-path=EFM/Engines --systems-path=EFM/Systems --init-path=autotest/init
```

`--fg-atmosphere` swaps the DCS path conventions for the FlightGear ones (aircraft in `.`,
engines and systems in `Engines/` and `Systems/`). The path options and `--property` come
from the fork's `DCS-WIP-no-hacks` branch, as does the `<check>` framework, so a stock
JSBSim build does not work yet.

Return codes: 0 = all checks passed, 1 = check failure, negative = config/load error.

Stdout is parsed with regex for:
- State Report sections (initial conditions)
- Event execution lines
- `CHECK PASS/FAIL` lines from JSBSim `<check>` elements
- `EVENT PASS/FAIL` lines, one per firing of an event that carries checks

## Exit Codes

- 0 = all tests pass
- 1 = any test failure
- 2 = runtime error

## Aircraft-Side Directory Layout

All test artifacts live under the aircraft mod directory, not this repo:

```
AircraftMod/autotest/
├── autotest.xml                    # Master config (new format)
├── scripts/<group>/<test>.xml      # JSBSim runscripts
├── init/                           # Initial condition files
├── data_output/                    # JSBSim output directives
├── output/                         # Generated: CSVs, plots, reports
└── validation-package/
    ├── tolerances.xml              # Tolerance definitions
    └── *.csv                       # Baseline CSVs
```
