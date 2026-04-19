#!/usr/bin/env python3

import argparse
import csv
import sys
import subprocess
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare two rlottie lottiebench builds on the same corpus."
    )
    parser.add_argument("--current-bin", default="build/example/lottiebench")
    parser.add_argument("--baseline-bin", required=True)
    parser.add_argument("--asset-dir", type=Path)
    parser.add_argument("--asset-list", type=Path)
    parser.add_argument("--size", action="append")
    parser.add_argument("--iterations", type=int, default=60)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not args.size:
        args.size = ["360x360"]
    return args


def load_assets(args):
    assets = []
    if args.asset_list:
        for line in args.asset_list.read_text().splitlines():
            value = line.strip()
            if not value or value.startswith("#"):
                continue
            assets.append(value)
    elif args.asset_dir:
        base = args.asset_dir.resolve()
        for path in sorted(base.rglob("*.json")):
            assets.append(str(path.relative_to(base)))
    else:
        raise SystemExit("Either --asset-dir or --asset-list is required.")

    if args.limit:
        assets = assets[: args.limit]
    return assets


def run_case(label, binary, asset_path, size, args):
    cmd = [
        binary,
        "--asset",
        str(asset_path),
        "--size",
        size,
        "--iterations",
        str(args.iterations),
        "--warmup",
        str(args.warmup),
        "--csv",
    ]

    proc = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False
    )

    header = None
    record = None
    for line in proc.stdout.splitlines():
        if line.startswith("asset,width,height,mode,frames,iterations,parse_ms"):
            header = line
            continue
        if header and line.count(",") >= 15:
            record = line

    row = {}
    if header and record:
        rows = list(csv.DictReader([header, record]))
        if rows:
            row = rows[0]

    row["renderer"] = label
    row["exit_code"] = str(proc.returncode)
    row["stderr"] = proc.stderr.strip().replace("\n", " | ")
    return row


def to_float(row, key):
    value = row.get(key, "")
    return float(value) if value not in ("", None) else None


def compare_rows(current_row, baseline_row):
    if not current_row or not baseline_row:
        return None
    if to_float(current_row, "avg_frame_ms") is None:
        return None
    if to_float(baseline_row, "avg_frame_ms") is None:
        return None

    summary = {}
    for metric in ("parse_ms", "first_frame_ms", "avg_frame_ms", "rss_steady_kb"):
        current_value = to_float(current_row, metric)
        baseline_value = to_float(baseline_row, metric)
        if current_value is None or baseline_value is None:
            summary[metric] = "unknown"
        elif abs(current_value - baseline_value) < 1e-9:
            summary[metric] = "tie"
        else:
            summary[metric] = (
                "current" if current_value < baseline_value else "baseline"
            )
    return summary


def main():
    args = parse_args()
    current_bin = Path(args.current_bin).resolve()
    baseline_bin = Path(args.baseline_bin).resolve()
    if not current_bin.exists():
        raise SystemExit(f"Missing current lottiebench: {current_bin}")
    if not baseline_bin.exists():
        raise SystemExit(f"Missing baseline lottiebench: {baseline_bin}")

    asset_base = args.asset_dir.resolve() if args.asset_dir else None
    assets = load_assets(args)
    rows = []
    summaries = []

    for asset in assets:
        asset_path = Path(asset)
        if asset_base and not asset_path.is_absolute():
            asset_path = asset_base / asset_path

        for size in args.size:
            current_row = run_case("current", str(current_bin), asset_path, size, args)
            baseline_row = run_case(
                "baseline", str(baseline_bin), asset_path, size, args
            )
            rows.extend([current_row, baseline_row])

            summary = compare_rows(current_row, baseline_row)
            if summary:
                summaries.append(summary)

    fieldnames = [
        "renderer",
        "asset",
        "width",
        "height",
        "mode",
        "frames",
        "iterations",
        "parse_ms",
        "first_frame_ms",
        "steady_ms",
        "avg_frame_ms",
        "fps",
        "rss_parse_kb",
        "rss_first_frame_kb",
        "rss_steady_kb",
        "nonzero_pixels",
        "alpha_sum",
        "red_sum",
        "green_sum",
        "blue_sum",
        "exit_code",
        "stderr",
    ]

    writer = csv.DictWriter(sys.stdout, fieldnames=fieldnames)
    writer.writeheader()
    for row in rows:
        writer.writerow({key: row.get(key, "") for key in fieldnames})

    if args.output:
        with args.output.open("w", newline="") as handle:
            file_writer = csv.DictWriter(handle, fieldnames=fieldnames)
            file_writer.writeheader()
            for row in rows:
                file_writer.writerow({key: row.get(key, "") for key in fieldnames})

    if summaries:
        winners = {
            "parse_ms": {"current": 0, "baseline": 0, "tie": 0, "unknown": 0},
            "first_frame_ms": {
                "current": 0,
                "baseline": 0,
                "tie": 0,
                "unknown": 0,
            },
            "avg_frame_ms": {"current": 0, "baseline": 0, "tie": 0, "unknown": 0},
            "rss_steady_kb": {
                "current": 0,
                "baseline": 0,
                "tie": 0,
                "unknown": 0,
            },
        }
        for summary in summaries:
            for metric, winner in summary.items():
                winners[metric][winner] += 1

        print("", file=sys.stderr)
        print("summary", file=sys.stderr)
        for metric, counts in winners.items():
            print(
                f"{metric}: current={counts['current']} baseline={counts['baseline']} tie={counts['tie']} unknown={counts['unknown']}",
                file=sys.stderr,
            )


if __name__ == "__main__":
    main()
