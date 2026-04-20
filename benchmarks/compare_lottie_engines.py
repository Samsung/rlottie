#!/usr/bin/env python3

import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compare rlottie and ThorVG on a shared Lottie corpus."
    )
    parser.add_argument("--rlottie-bin", default="build/example/lottiebench")
    parser.add_argument("--thorvg-bin", default="build/example/thorvgbench")
    parser.add_argument("--asset-dir", type=Path)
    parser.add_argument("--asset-list", type=Path)
    parser.add_argument("--size", action="append")
    parser.add_argument("--iterations", type=int, default=60)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not args.size:
        args.size = ["360x360"]
    return args


def load_assets(args):
    assets = []
    base = args.asset_dir.resolve() if args.asset_dir else None
    if args.asset_list:
        for line in args.asset_list.read_text().splitlines():
            value = line.strip()
            if not value or value.startswith("#"):
                continue
            path = Path(value)
            if base and not path.is_absolute():
                path = base / path
            assets.append(str(path))
    elif args.asset_dir:
        for path in sorted(base.rglob("*.json")):
            assets.append(str(path.relative_to(base)))
    else:
        raise SystemExit("Either --asset-dir or --asset-list is required.")

    if args.limit:
        assets = assets[: args.limit]
    return assets


def run_case(engine, binary, asset_path, size, args):
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
    if engine == "thorvg" and args.threads is not None:
        cmd.extend(["--threads", str(args.threads)])

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

    row["renderer"] = engine
    row["exit_code"] = str(proc.returncode)
    row["stderr"] = proc.stderr.strip().replace("\n", " | ")
    return row


def to_float(row, key):
    value = row.get(key, "")
    return float(value) if value not in ("", None) else None


def to_int(row, key):
    value = row.get(key, "")
    return int(float(value)) if value not in ("", None) else None


def compare_rows(rlottie_row, thorvg_row):
    if not rlottie_row or not thorvg_row:
        return None
    if to_int(rlottie_row, "frames") in (None, 0) or to_int(thorvg_row, "frames") in (None, 0):
        return None

    summary = {}
    for metric in ("parse_ms", "first_frame_ms", "avg_frame_ms", "rss_steady_kb"):
        r_value = to_float(rlottie_row, metric)
        t_value = to_float(thorvg_row, metric)
        if r_value is None or t_value is None:
            summary[metric] = "unknown"
        elif abs(r_value - t_value) < 1e-9:
            summary[metric] = "tie"
        else:
            summary[metric] = "rlottie" if r_value < t_value else "thorvg"
    return summary


def row_key(row):
    return (
        row.get("renderer", ""),
        row.get("asset", ""),
        row.get("width", ""),
        row.get("height", ""),
        row.get("mode", ""),
    )


def median(values):
    ordered = sorted(values)
    if not ordered:
        return None
    mid = len(ordered) // 2
    if len(ordered) % 2 == 1:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2.0


def aggregate_rows(rows):
    grouped = {}
    metrics = (
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
    )

    for row in rows:
        key = row_key(row)
        grouped.setdefault(key, []).append(row)

    aggregated = []
    for samples in grouped.values():
        base = dict(samples[0])
        base["trial_count"] = str(len(samples))
        for metric in metrics:
            numeric = [to_float(sample, metric) for sample in samples]
            numeric = [value for value in numeric if value is not None]
            if numeric:
                base[metric] = str(median(numeric))
                base[f"{metric}_min"] = str(min(numeric))
                base[f"{metric}_max"] = str(max(numeric))
            else:
                base[metric] = ""
                base[f"{metric}_min"] = ""
                base[f"{metric}_max"] = ""
        exit_codes = [to_int(sample, "exit_code") for sample in samples]
        exit_codes = [code for code in exit_codes if code is not None]
        base["exit_code"] = str(max(exit_codes)) if exit_codes else ""
        stderr = [sample.get("stderr", "") for sample in samples if sample.get("stderr", "")]
        base["stderr"] = " || ".join(stderr)
        aggregated.append(base)

    aggregated.sort(
        key=lambda row: (
            row.get("asset", ""),
            int(row.get("width", "0") or 0),
            int(row.get("height", "0") or 0),
            row.get("renderer", ""),
        )
    )
    return aggregated


def main():
    args = parse_args()
    rlottie_bin = Path(args.rlottie_bin).resolve()
    thorvg_bin = Path(args.thorvg_bin).resolve()
    if not rlottie_bin.exists():
        raise SystemExit(f"Missing rlottie bench: {rlottie_bin}")
    if not thorvg_bin.exists():
        raise SystemExit(f"Missing thorvg bench: {thorvg_bin}")

    asset_base = args.asset_dir.resolve() if args.asset_dir else None
    assets = load_assets(args)
    trial_rows = []
    summaries = []

    for trial in range(args.trials):
        order = ("rlottie", "thorvg") if trial % 2 == 0 else ("thorvg", "rlottie")
        for asset in assets:
            asset_path = Path(asset)
            if asset_base and not asset_path.is_absolute():
                asset_path = asset_base / asset_path

            for size in args.size:
                pair = {}
                for engine in order:
                    if engine == "rlottie":
                        row = run_case("rlottie", str(rlottie_bin), asset_path, size, args)
                    else:
                        row = run_case("thorvg", str(thorvg_bin), asset_path, size, args)
                    row["trial_index"] = str(trial)
                    trial_rows.append(row)
                    pair[engine] = row

                summary = compare_rows(pair.get("rlottie"), pair.get("thorvg"))
                if summary:
                    summaries.append(summary)

    rows = aggregate_rows(trial_rows)
    for row in rows:
        row.pop("trial_index", None)

    if summaries:
        winners = {
            "parse_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "first_frame_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "avg_frame_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "rss_steady_kb": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
        }
        for summary in summaries:
            for metric, winner in summary.items():
                winners[metric][winner] += 1

        aggregate_summaries = []
        rows_by_case = {}
        for row in rows:
            case = (row.get("asset"), row.get("width"), row.get("height"), row.get("mode"))
            rows_by_case.setdefault(case, {})[row.get("renderer")] = row
        for pair in rows_by_case.values():
            summary = compare_rows(pair.get("rlottie"), pair.get("thorvg"))
            if summary:
                aggregate_summaries.append(summary)

        aggregate_winners = {
            "parse_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "first_frame_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "avg_frame_ms": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
            "rss_steady_kb": {"rlottie": 0, "thorvg": 0, "tie": 0, "unknown": 0},
        }
        for summary in aggregate_summaries:
            for metric, winner in summary.items():
                aggregate_winners[metric][winner] += 1

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
        "trial_count",
        "parse_ms_min",
        "parse_ms_max",
        "first_frame_ms_min",
        "first_frame_ms_max",
        "steady_ms_min",
        "steady_ms_max",
        "avg_frame_ms_min",
        "avg_frame_ms_max",
        "rss_steady_kb_min",
        "rss_steady_kb_max",
        "exit_code",
        "stderr",
    ]

    output = sys.stdout
    writer = csv.DictWriter(output, fieldnames=fieldnames)
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
        print("", file=sys.stderr)
        print(f"summary (trial-level, trials={args.trials})", file=sys.stderr)
        for metric, counts in winners.items():
            print(
                f"{metric}: rlottie={counts['rlottie']} thorvg={counts['thorvg']} tie={counts['tie']} unknown={counts['unknown']}",
                file=sys.stderr,
            )
        print("", file=sys.stderr)
        print("summary (median-case)", file=sys.stderr)
        for metric, counts in aggregate_winners.items():
            print(
                f"{metric}: rlottie={counts['rlottie']} thorvg={counts['thorvg']} tie={counts['tie']} unknown={counts['unknown']}",
                file=sys.stderr,
            )


if __name__ == "__main__":
    main()
