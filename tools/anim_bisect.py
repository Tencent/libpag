#!/usr/bin/env python3
"""Attribute a failing composite animation case to the channels that break it.

A composite case drives many channels on one element, so a low score does not say which channel is at
fault. This script re-runs the frame eval once per channel, with that channel removed from the XML:

* a variant whose score jumps back above `--threshold` means that channel is (part of) the culprit;
* a variant whose score stays low means the channel is not involved in the failure.

Every variant is a real eval run, so the whole bisect costs `channels + 1` runs (about 7 s each with
the default 8 samples).

    python3 tools/anim_bisect.py resources/pagx_to_html/animations/composite/06_painter.pagx
    python3 tools/anim_bisect.py <case.pagx> --threshold 0.99 --samples 8 -j 4
"""

import argparse
import csv
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EVAL_DIR = os.path.join(REPO, "tools/html-snapshot/eval-animation")
CHANNEL_RE = re.compile(r'\n\s*<Channel name="(?P<name>[^"]+)" type="[^"]+">.*?</Channel>', re.S)
OBJECT_RE = re.compile(r'(\s*)<Object target="(?P<target>[^"]+)">(?P<body>.*?)</Object>', re.S)


def channels(text):
    """Yields (target, channel, occurrence index) for every Channel element."""
    found = []
    for obj in OBJECT_RE.finditer(text):
        for ch in CHANNEL_RE.finditer(obj.group("body")):
            found.append((obj.group("target"), ch.group("name"), obj.start(), ch.start(), ch.end()))
    return found


def remove(text, target, index):
    """Returns the document with the `index`-th channel of `target` removed."""
    for obj in OBJECT_RE.finditer(text):
        if obj.group("target") != target:
            continue
        body = obj.group("body")
        seen = -1
        for ch in CHANNEL_RE.finditer(body):
            seen += 1
            if seen != index:
                continue
            new_body = body[: ch.start()] + body[ch.end():]
            new_obj = obj.group(0)
            if new_body.strip():
                new_obj = new_obj.replace(body, new_body, 1)
            # An Object with no channel left is dropped together with its whitespace.
            return text[: obj.start()] + new_obj + text[obj.end():]
    raise SystemExit("channel not found: %s #%d" % (target, index))


def run_eval(case_path, work_dir, samples, jobs):
    """Copies the case into a private corpus, runs the eval and returns ssimMean.

    The report grades on the per-frame mean alone; `ssimMin` and the static-baseline columns were
    removed, so this reads the one metric that still exists.
    """
    corpus = os.path.join(work_dir, "corpus")
    out = os.path.join(work_dir, "out")
    os.makedirs(corpus, exist_ok=True)
    name = os.path.basename(case_path)
    shutil.copyfile(case_path, os.path.join(corpus, name))
    result = subprocess.run(
        ["node", "run.js", "--source", "pagx", "-r", "-j", str(jobs), "--samples", str(samples),
         "--corpus", corpus, "--out", out],
        cwd=EVAL_DIR, capture_output=True, text=True,
    )
    report = os.path.join(out, "report.csv")
    if not os.path.exists(report):
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit("eval produced no report: %s" % case_path)
    with open(report, newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or not rows[0].get("ssimMean"):
        raise SystemExit("no ssim in report: %s" % case_path)
    return float(rows[0]["ssimMean"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("case")
    parser.add_argument("--threshold", type=float, default=0.99)
    parser.add_argument("--samples", type=int, default=8)
    parser.add_argument("-j", "--jobs", type=int, default=4)
    args = parser.parse_args()

    case_path = os.path.abspath(args.case)
    text = open(case_path).read()
    found = channels(text)
    if not found:
        raise SystemExit("no Channel elements: %s" % case_path)

    with tempfile.TemporaryDirectory() as tmp:
        base_mean = run_eval(case_path, os.path.join(tmp, "base"), args.samples, args.jobs)
        print("%s\n  baseline  mean=%.4f  (threshold %.2f)\n" % (
            os.path.basename(case_path), base_mean, args.threshold))

        rows = []
        per_target = {}
        for target, name, _obj_start, _start, _end in found:
            index = per_target.get(target, 0)
            per_target[target] = index + 1
            variant = remove(text, target, index)
            variant_path = os.path.join(tmp, "v", os.path.basename(case_path))
            os.makedirs(os.path.dirname(variant_path), exist_ok=True)
            open(variant_path, "w").write(variant)
            mean = run_eval(variant_path, os.path.join(tmp, "v%d" % len(rows)), args.samples,
                            args.jobs)
            rows.append((target, name, mean))

        rows.sort(key=lambda row: -row[2])
        print("  drop channel            mean    verdict")
        for target, name, mean in rows:
            verdict = "CULPRIT" if mean >= args.threshold else "no effect"
            print("  %-20s %-14s %.4f  %s" % (target, name, mean, verdict))

    return 0


if __name__ == "__main__":
    sys.exit(main())
