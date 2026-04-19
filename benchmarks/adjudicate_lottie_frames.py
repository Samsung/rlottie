#!/usr/bin/env python3

import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def run_dump(cmd):
    completed = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=True,
    )
    return completed


def extract_metrics_line(stdout: str) -> str:
    lines = [line for line in stdout.splitlines() if line.strip()]
    return lines[-1] if lines else ""


def convert_ppm_to_png(src: Path, dst: Path) -> None:
    with Image.open(src) as image:
        image.save(dst)


def compose_side_by_side(left: Image.Image, right: Image.Image) -> Image.Image:
    width = left.width + right.width
    height = max(left.height, right.height)
    canvas = Image.new("RGB", (width, height), (18, 22, 28))
    canvas.paste(left, (0, 0))
    canvas.paste(right, (left.width, 0))
    return canvas


def diff_metrics(left: Image.Image, right: Image.Image):
    diff = ImageChops.difference(left, right)
    stat = ImageStat.Stat(diff)
    bbox = diff.getbbox()
    nonzero = 0
    if bbox:
        pixels = diff.load()
        for y in range(diff.height):
            for x in range(diff.width):
                if pixels[x, y] != (0, 0, 0):
                    nonzero += 1

    total = left.width * left.height
    return {
        "exact_match_ratio": 0.0 if total == 0 else (total - nonzero) / total,
        "diff_pixel_count": nonzero,
        "diff_bbox": bbox,
        "mean_abs_diff_rgb": stat.mean,
        "rms_diff_rgb": stat.rms,
        "max_abs_diff_rgb": stat.extrema,
        "diff_image": diff,
    }


def parse_args():
    parser = argparse.ArgumentParser(
        description="Dump and compare rlottie/thorvg first-frame images."
    )
    parser.add_argument("--asset", required=True)
    parser.add_argument("--size", default="360x360")
    parser.add_argument(
        "--rlottie-bin",
        default="build/example/lottiebench",
    )
    parser.add_argument(
        "--thorvg-bin",
        default="build/example/thorvgbench",
    )
    parser.add_argument("--threads", type=int, default=None)
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Directory to store rendered pngs, diff png, and metrics json",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="lottie-adjudicate-") as temp_dir:
        temp_dir = Path(temp_dir)
        rlottie_ppm = temp_dir / "rlottie.ppm"
        thorvg_ppm = temp_dir / "thorvg.ppm"

        rlottie_cmd = [
            args.rlottie_bin,
            "--asset",
            args.asset,
            "--size",
            args.size,
            "--iterations",
            "1",
            "--warmup",
            "0",
            "--dump-first-frame",
            str(rlottie_ppm),
            "--csv",
        ]
        thorvg_cmd = [
            args.thorvg_bin,
            "--asset",
            args.asset,
            "--size",
            args.size,
            "--iterations",
            "1",
            "--warmup",
            "0",
            "--dump-first-frame",
            str(thorvg_ppm),
            "--csv",
        ]
        if args.threads:
            thorvg_cmd.extend(["--threads", str(args.threads)])

        rlottie_run = run_dump(rlottie_cmd)
        thorvg_run = run_dump(thorvg_cmd)

        rlottie_png = output_dir / "rlottie_first_frame.png"
        thorvg_png = output_dir / "thorvg_first_frame.png"
        diff_png = output_dir / "diff_first_frame.png"
        side_png = output_dir / "side_by_side.png"
        metrics_json = output_dir / "metrics.json"
        rlottie_csv = output_dir / "rlottie_metrics.csv"
        thorvg_csv = output_dir / "thorvg_metrics.csv"

        convert_ppm_to_png(rlottie_ppm, rlottie_png)
        convert_ppm_to_png(thorvg_ppm, thorvg_png)

        with Image.open(rlottie_png) as rlottie_image, Image.open(thorvg_png) as thorvg_image:
            rlottie_rgb = rlottie_image.convert("RGB")
            thorvg_rgb = thorvg_image.convert("RGB")
            metrics = diff_metrics(rlottie_rgb, thorvg_rgb)
            metrics["asset"] = args.asset
            metrics["size"] = args.size
            metrics["rlottie_command"] = rlottie_cmd
            metrics["thorvg_command"] = thorvg_cmd
            metrics["rlottie_metrics_line"] = extract_metrics_line(rlottie_run.stdout)
            metrics["thorvg_metrics_line"] = extract_metrics_line(thorvg_run.stdout)
            metrics["rlottie_stdout"] = rlottie_run.stdout.strip()
            metrics["thorvg_stdout"] = thorvg_run.stdout.strip()
            metrics["rlottie_stderr"] = rlottie_run.stderr.strip()
            metrics["thorvg_stderr"] = thorvg_run.stderr.strip()

            metrics["diff_image"].save(diff_png)
            compose_side_by_side(rlottie_rgb, thorvg_rgb).save(side_png)
            del metrics["diff_image"]

            with open(metrics_json, "w", encoding="utf-8") as stream:
                json.dump(metrics, stream, indent=2)

        rlottie_csv.write_text(rlottie_run.stdout, encoding="utf-8")
        thorvg_csv.write_text(thorvg_run.stdout, encoding="utf-8")

    print(json.dumps({
        "asset": args.asset,
        "size": args.size,
        "output_dir": str(output_dir),
        "metrics_json": str(metrics_json),
        "rlottie_png": str(rlottie_png),
        "thorvg_png": str(thorvg_png),
        "diff_png": str(diff_png),
        "side_by_side_png": str(side_png),
    }, indent=2))


if __name__ == "__main__":
    main()
