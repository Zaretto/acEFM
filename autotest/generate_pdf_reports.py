#!/usr/bin/env python3
"""
Generate PDF reports from autotest XML output using Apache FOP.

Transforms autotest XML reports via XSLT stylesheets to XSL-FO, then
invokes Apache FOP to render PDFs.

Usage:
    # Generate all group PDFs
    python generate_pdf_reports.py --aircraft path/to/AircraftMod

    # Generate summary PDF only
    python generate_pdf_reports.py --aircraft path/to/AircraftMod --summary

    # Generate specific group(s)
    python generate_pdf_reports.py --aircraft path/to/AircraftMod electrics hydraulics

    # Specify FOP path explicitly
    python generate_pdf_reports.py --aircraft path/to/AircraftMod --fop path/to/fop.bat
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from xml.etree import ElementTree as ET


_NS = "urn:aviastorm:autotest-results:1.0"


def find_fop(fop_arg=None):
    """Locate the Apache FOP executable."""
    if fop_arg:
        fop = Path(fop_arg)
        if fop.exists():
            return str(fop)
        print(f"ERROR: FOP not found at {fop}")
        sys.exit(2)

    # Search PATH for fop / fop.bat / fop.cmd
    for name in ("fop", "fop.bat", "fop.cmd"):
        found = shutil.which(name)
        if found:
            return found

    print("ERROR: Apache FOP not found on PATH. Install FOP or use --fop.")
    sys.exit(2)


def get_document_number(xml_path):
    """Extract document-number from an autotest XML report for PDF naming."""
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        ns = {"at": _NS}
        dn_elem = root.find("at:metadata/at:document-number", ns)
        if dn_elem is not None and dn_elem.text and dn_elem.text.strip():
            return dn_elem.text.strip()
    except ET.ParseError:
        pass
    return None


def run_fop(fop_exe, xml_path, xslt_path, pdf_path):
    """Invoke Apache FOP to transform XML via XSLT to PDF.

    Returns True on success, False on failure.
    """
    cmd = [
        fop_exe,
        "-xml", str(xml_path),
        "-xsl", str(xslt_path),
        "-pdf", str(pdf_path),
    ]
    print(f"  FOP: {xml_path.name} -> {pdf_path.name}")
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode != 0:
            print(f"    FOP ERROR (rc={result.returncode}):")
            if result.stderr:
                # Show last few lines of stderr
                for line in result.stderr.strip().splitlines()[-5:]:
                    print(f"      {line}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print("    FOP timed out after 120 seconds")
        return False
    except FileNotFoundError:
        print(f"    FOP executable not found: {fop_exe}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Generate PDF reports from autotest XML via Apache FOP",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--aircraft",
        required=True,
        help="Path to aircraft mod directory",
    )
    parser.add_argument(
        "--fop",
        default=None,
        help="Path to Apache FOP executable (if not on PATH)",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="Also generate summary PDF",
    )
    parser.add_argument(
        "groups",
        nargs="*",
        help="Specific group names to generate (default: all)",
    )

    args = parser.parse_args()

    fop_exe = find_fop(args.fop)
    aircraft_dir = Path(args.aircraft).resolve()
    output_dir = aircraft_dir / "autotest" / "output"

    # Locate XSLT stylesheets relative to this script
    script_dir = Path(__file__).resolve().parent
    report_xslt = script_dir / "schema" / "autotest-report-fo.xslt"
    summary_xslt = script_dir / "schema" / "autotest-summary-fo.xslt"

    if not report_xslt.exists():
        print(f"ERROR: Report XSLT not found: {report_xslt}")
        sys.exit(2)

    if not output_dir.exists():
        print(f"ERROR: Output directory not found: {output_dir}")
        print("Run run_validation.py first to generate XML reports.")
        sys.exit(2)

    # Collect XML report files
    xml_files = sorted(output_dir.glob("*.xml"))
    # Exclude summary.xml from group reports
    group_xmls = [f for f in xml_files if f.name != "summary.xml"]
    summary_xml = output_dir / "summary.xml"

    if args.groups:
        # Filter to requested groups
        group_xmls = [f for f in group_xmls if f.stem in args.groups]
        if not group_xmls:
            print(f"ERROR: No XML files found for groups: {', '.join(args.groups)}")
            sys.exit(2)

    if not group_xmls and not (args.summary and summary_xml.exists()):
        print("No XML report files found in output directory.")
        print("Run run_validation.py first to generate XML reports.")
        sys.exit(2)

    print(f"Using FOP: {fop_exe}")
    print(f"Processing {len(group_xmls)} group report(s)...")
    print()

    success = 0
    failed = 0

    for xml_path in group_xmls:
        # Try to name PDF by document number, fall back to group name
        doc_num = get_document_number(xml_path)
        if doc_num:
            pdf_name = f"{doc_num}.pdf"
        else:
            pdf_name = f"{xml_path.stem}.pdf"

        pdf_path = output_dir / pdf_name
        if run_fop(fop_exe, xml_path, report_xslt, pdf_path):
            success += 1
        else:
            failed += 1

    # Summary PDF
    if args.summary and summary_xml.exists():
        if not summary_xslt.exists():
            print(f"WARNING: Summary XSLT not found: {summary_xslt}")
        else:
            pdf_path = output_dir / "validation-summary.pdf"
            if run_fop(fop_exe, summary_xml, summary_xslt, pdf_path):
                success += 1
            else:
                failed += 1

    print()
    print(f"Done: {success} PDF(s) generated, {failed} failed.")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
