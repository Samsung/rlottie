#!/usr/bin/env python3

import argparse
import csv
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run and summarize an rlottie-vs-ThorVG audit over a Lottie corpus."
    )
    parser.add_argument("--asset-dir", type=Path, required=True)
    parser.add_argument("--asset-list", type=Path)
    parser.add_argument("--rlottie-bin", default="build/example/lottiebench")
    parser.add_argument("--thorvg-bin", default="build/example/thorvgbench")
    parser.add_argument(
        "--compare-script",
        type=Path,
        default=Path("benchmarks/compare_lottie_engines.py"),
    )
    parser.add_argument("--size", action="append")
    parser.add_argument("--iterations", type=int, default=10)
    parser.add_argument("--warmup", type=int, default=2)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--top", type=int, default=10)
    parser.add_argument("--output-csv", type=Path)
    args = parser.parse_args()
    if not args.size:
        args.size = ["360x360"]
    return args


def run_compare(args, output_csv):
    cmd = [
        sys.executable,
        str(args.compare_script.resolve()),
        "--asset-dir",
        str(args.asset_dir.resolve()),
        "--rlottie-bin",
        str(Path(args.rlottie_bin).resolve()),
        "--thorvg-bin",
        str(Path(args.thorvg_bin).resolve()),
        "--iterations",
        str(args.iterations),
        "--warmup",
        str(args.warmup),
        "--output",
        str(output_csv.resolve()),
    ]
    if args.asset_list:
        cmd.extend(["--asset-list", str(args.asset_list.resolve())])
    if args.threads is not None:
        cmd.extend(["--threads", str(args.threads)])
    if args.trials is not None:
        cmd.extend(["--trials", str(args.trials)])
    if args.limit is not None:
        cmd.extend(["--limit", str(args.limit)])
    for size in args.size:
        cmd.extend(["--size", size])

    proc = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False
    )
    if proc.returncode != 0:
        raise SystemExit(proc.stderr.strip() or proc.stdout.strip())
    return proc.stderr.strip()


def load_rows(csv_path):
    with csv_path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(row, key):
    value = row.get(key, "")
    return float(value) if value not in ("", None) else None


def to_int(row, key):
    value = row.get(key, "")
    return int(float(value)) if value not in ("", None) else None


def is_loaded(row):
    return to_int(row, "exit_code") == 0 and (to_int(row, "frames") or 0) > 0


def case_label(row):
    return (
        f"{row.get('asset','?')} @ {row.get('width','?')}x{row.get('height','?')}"
        f" {row.get('mode','?')}"
    )


def signature_tuple(row):
    return tuple(to_int(row, key) or 0 for key in (
        "nonzero_pixels",
        "alpha_sum",
        "red_sum",
        "green_sum",
        "blue_sum",
    ))


def print_section(title, lines):
    print(f"\n{title}")
    if not lines:
        print("  none")
        return
    for line in lines:
        print(f"  {line}")


def main():
    args = parse_args()

    with tempfile.NamedTemporaryFile(
        prefix="rlottie-thorvg-audit-", suffix=".csv", delete=False
    ) as handle:
        temp_csv = Path(handle.name)

    output_csv = args.output_csv or temp_csv
    compare_summary = run_compare(args, output_csv)
    rows = load_rows(output_csv)

    grouped = defaultdict(dict)
    for row in rows:
        key = (
            row.get("asset", ""),
            row.get("width", ""),
            row.get("height", ""),
            row.get("mode", ""),
        )
        grouped[key][row.get("renderer", "")] = row

    total_cases = len(grouped)
    loaded_both = []
    rlottie_failures = []
    thorvg_failures = []
    signature_mismatches = []
    avg_losses = []
    parse_losses = []
    first_frame_losses = []
    rss_losses = []

    for _, pair in grouped.items():
        rlottie = pair.get("rlottie")
        thorvg = pair.get("thorvg")
        if not rlottie or not thorvg:
            continue

        label = case_label(rlottie)
        rl_loaded = is_loaded(rlottie)
        th_loaded = is_loaded(thorvg)

        if not rl_loaded:
            rlottie_failures.append(
                f"{label}: frames={rlottie.get('frames','')} exit={rlottie.get('exit_code','')} stderr={rlottie.get('stderr','')}"
            )
        if not th_loaded:
            thorvg_failures.append(
                f"{label}: frames={thorvg.get('frames','')} exit={thorvg.get('exit_code','')} stderr={thorvg.get('stderr','')}"
            )

        if not (rl_loaded and th_loaded):
            continue

        loaded_both.append(label)

        if signature_tuple(rlottie) != signature_tuple(thorvg):
            signature_mismatches.append(
                f"{label}: rlottie={signature_tuple(rlottie)} thorvg={signature_tuple(thorvg)}"
            )

        metrics = [
            ("avg_frame_ms", avg_losses),
            ("parse_ms", parse_losses),
            ("first_frame_ms", first_frame_losses),
            ("rss_steady_kb", rss_losses),
        ]
        for metric, bucket in metrics:
            r_value = to_float(rlottie, metric)
            t_value = to_float(thorvg, metric)
            if r_value is None or t_value is None:
                continue
            delta = r_value - t_value
            if delta > 0:
                bucket.append((delta, label, r_value, t_value))

    def top_lines(bucket, fmt):
        bucket.sort(key=lambda item: item[0], reverse=True)
        return [fmt(*item) for item in bucket[: args.top]]

    print("audit summary")
    print(f"  total cases: {total_cases}")
    print(f"  common loaded cases: {len(loaded_both)}")
    print(f"  rlottie load failures: {len(rlottie_failures)}")
    print(f"  thorvg load failures: {len(thorvg_failures)}")
    print(f"  common-case signature mismatches: {len(signature_mismatches)}")

    if compare_summary:
        print("\ncomparison summary")
        for line in compare_summary.splitlines():
            print(f"  {line}")

    print_section("rlottie load failures", rlottie_failures[: args.top])
    print_section("thorvg load failures", thorvg_failures[: args.top])
    print_section("signature mismatches", signature_mismatches[: args.top])
    print_section(
        "top rlottie steady-state losses",
        top_lines(
            avg_losses,
            lambda delta, label, r_value, t_value: (
                f"{label}: rlottie={r_value:.6f} ms thorvg={t_value:.6f} ms "
                f"delta={delta:.6f}"
            ),
        ),
    )
    print_section(
        "top rlottie parse losses",
        top_lines(
            parse_losses,
            lambda delta, label, r_value, t_value: (
                f"{label}: rlottie={r_value:.6f} ms thorvg={t_value:.6f} ms "
                f"delta={delta:.6f}"
            ),
        ),
    )
    print_section(
        "top rlottie first-frame losses",
        top_lines(
            first_frame_losses,
            lambda delta, label, r_value, t_value: (
                f"{label}: rlottie={r_value:.6f} ms thorvg={t_value:.6f} ms "
                f"delta={delta:.6f}"
            ),
        ),
    )
    print_section(
        "top rlottie RSS losses",
        top_lines(
            rss_losses,
            lambda delta, label, r_value, t_value: (
                f"{label}: rlottie={r_value:.0f} KB thorvg={t_value:.0f} KB "
                f"delta={delta:.0f}"
            ),
        ),
    )

    if args.output_csv:
        print(f"\ncomparison csv: {args.output_csv.resolve()}")


if __name__ == "__main__":
    main()
