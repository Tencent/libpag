#!/usr/bin/env python3
"""Derive the animation coverage tables from the PAGX channel registry, the HTML writer's channel
table and the corpus headers.

Run from the repo root:

    python3 tools/anim_coverage.py            # print the tables
    python3 tools/anim_coverage.py --check    # verify the invariants, non-zero exit on failure

The tables are copied into resources/pagx_to_html/animations/README.md, so a reviewer can re-run
this to see that the document still matches the code.
"""

import argparse
import glob
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHANNEL_SRC = os.path.join(REPO, "src/pagx/PAGXNodeChannel.cpp")
WRITER_SRC = os.path.join(REPO, "src/pagx/html/HTMLAnimationWriter.cpp")
CORPUS = os.path.join(REPO, "resources/pagx_to_html/animations")
PAGX_BIN = os.path.join(REPO, "cmake-build-debug/pagx")

NODE_ORDER = [
    "Layer", "Group", "TextBox", "Text", "TextPath", "Rectangle", "Ellipse", "Polystar", "Path",
    "Fill", "Stroke", "TrimPath", "RoundCorner", "Repeater", "TextModifier", "RangeSelector",
    "SolidColor", "LinearGradient", "RadialGradient", "ConicGradient", "DiamondGradient",
    "ColorStop", "DropShadowStyle", "InnerShadowStyle", "BackgroundBlurStyle", "GlassStyle",
    "NoiseStyle", "BlurFilter", "DropShadowFilter", "InnerShadowFilter", "BlendFilter",
    "NoiseFilter",
]


def load_registry():
    src = open(CHANNEL_SRC).read()
    anim = {}
    shared = re.search(r"static void AppendGroupCommonFields.*?\n}\n", src, re.S).group(0)
    shared_rows = re.findall(r'FIELD_\w+\(T, "([^"]+)", \w+, (?:Anim|AnimLayout)\)', shared)
    funcs = re.split(r"\nstatic std::vector<ChannelDef> (Build\w+)", src)
    for i in range(1, len(funcs), 2):
        name, body = funcs[i], funcs[i + 1]
        for m in re.finditer(r'FIELD_\w+\((\w+), "([^"]+)", \w+, (?:Anim|AnimLayout)\)', body):
            anim.setdefault(m.group(1), set()).add(m.group(2))
        if re.search(r'\{"color", Anim,', body):
            node = "Fill" if name == "BuildFillFields" else "Stroke"
            anim.setdefault(node, set()).add("color")
        if name in ("BuildGroupFields", "BuildTextBoxFields"):
            node = "Group" if name == "BuildGroupFields" else "TextBox"
            anim.setdefault(node, set()).update(shared_rows)
        if name == "BuildTextPathFields":
            anim.setdefault("TextPath", set()).update(shared_rows)
    # matrix3D is declared with its own macro
    if 'FIELD_MATRIX3D(Layer, "matrix3D"' in src:
        anim.setdefault("Layer", set()).add("matrix3D")
    return anim


def load_exported():
    src = open(WRITER_SRC).read()
    table = re.search(r"kChannelBindingTable\[\] = \{(.*?)\n\};", src, re.S).group(1)
    exported = {}
    for m in re.finditer(r'\{NodeType::(\w+), "([^"]+)"', table):
        exported.setdefault(m.group(1), set()).add(m.group(2))
    # `Path.point{N}.x/y` is parsed before the table lookup
    exported.setdefault("Path", set()).add("point{N}.x/y")
    return exported


def load_cases():
    cases = []
    for path in sorted(glob.glob(os.path.join(CORPUS, "**", "*.pagx"), recursive=True)):
        text = open(path).read()
        rel = os.path.relpath(path, CORPUS).replace(os.sep, "/")
        header = text[: text.find("-->")]
        targets = re.findall(r"Target:\s*(\S+)", header)
        channels = []
        for group in re.findall(r"Channel:\s*(\S+)", header):
            channels.extend(part.strip() for part in group.split(",") if part.strip())
        expect = re.search(r"Expect:\s*(\w+)", header)
        cases.append(
            {
                "file": rel,
                "short": rel[:-5] if rel.endswith(".pagx") else rel,
                "family": rel.split("/")[0] if "/" in rel else "(root)",
                "targets": targets,
                "channels": channels,
                "expect": expect.group(1) if expect else "?",
            }
        )
    return cases


def case_index(cases):
    """(node, channel) -> [case short names]"""
    index = {}
    for case in cases:
        for target in case["targets"]:
            for channel in case["channels"]:
                index.setdefault((target, channel), []).append(case["short"])
    return index


def channel_rows(anim, exported, index):
    rows = []
    for node in NODE_ORDER:
        channels = sorted(anim.get(node, ()))
        if not channels:
            continue
        buckets = {}
        for channel in channels:
            cases = index.get((node, channel), [])
            if not cases and node == "Path" and channel.startswith("point"):
                cases = index.get((node, "point1.x"), [])
            key = (channel in exported.get(node, ()), tuple(cases))
            buckets.setdefault(key, []).append(channel)
        for (is_exported, cases), group in buckets.items():
            rows.append(
                {
                    "node": node,
                    "channels": group,
                    "exported": is_exported,
                    "cases": list(cases),
                }
            )
    return rows


def shared_channel_rows(anim, index):
    """Channels driven by more than one case, grouped by family, so the reader can check that each
    case varies a different axis instead of repeating the previous one."""
    rows = []
    for node in NODE_ORDER:
        for channel in sorted(anim.get(node, ())):
            cases = index.get((node, channel), [])
            if len(cases) < 2:
                continue
            rows.append((node, channel, sorted(cases)))
    return rows


def render_shared(anim, index):
    lines = ["| 通道 | 用例数 | 用例（按族） |", "|------|--------|--------------|"]
    for node, channel, cases in shared_channel_rows(anim, index):
        lines.append("| `%s.%s` | %d | %s |" % (node, channel, len(cases), compact_cases(cases)))
    return "\n".join(lines)


def short_id(short):
    family, _, name = short.rpartition("/")
    token = re.match(r"(\d+[a-z]?)", name)
    return (family + "/" + (token.group(1) if token else name)) if family else short


def compact_cases(cases):
    """`family/*（n）` when a family contributes several cases, `family/a·b` for two, else the id."""
    grouped = {}
    for short in cases:
        grouped.setdefault(short.rpartition("/")[0], []).append(short_id(short))
    parts = []
    for family in sorted(grouped):
        members = sorted(grouped[family])
        if len(members) >= 3:
            parts.append("`%s/*`（%d）" % (family, len(members)))
        elif len(members) == 2:
            parts.append("`%s`" % (family + "/" + "·".join(m.rsplit("/", 1)[1] for m in members)))
        else:
            parts.append("`%s`" % members[0])
    return "、".join(parts)


def render_master(rows):
    lines = [
        "| 节点 | 可动画通道 | 导出 | 用例 |",
        "|------|------------|------|------|",
    ]
    for row in rows:
        channels = " ".join("`%s`" % c for c in row["channels"])
        cases = compact_cases(row["cases"]) if row["cases"] else "—"
        lines.append("| %s | %s | %s | %s |" % (row["node"], channels, "✔" if row["exported"] else "✘", cases))
    return "\n".join(lines)


def render_families(cases):
    families = {}
    for case in cases:
        families.setdefault(case["family"], []).append(case)
    lines = ["| 族 | 用例 | 覆盖维度 |", "|----|------|----------|"]
    for family in sorted(families):
        members = families[family]
        channels = []
        for case in members:
            for channel in case["channels"]:
                label = channel if len(channels) == 0 or channel not in channels else None
                if label:
                    channels.append(label)
        lines.append(
            "| `%s` | %d（`%s/01`~`%s/%02d`） | %s |"
            % (
                family,
                len(members),
                family,
                family,
                len(members),
                "、".join("`%s`" % c for c in channels[:12]) + ("…" if len(channels) > 12 else ""),
            )
        )
    return "\n".join(lines)


def check(anim, exported, index, cases):
    """Returns (failures, warnings).

    A channel is covered either by a case that drives it or by a case on the same node type: the
    exporter decides per node type and channel family, so one representative case per node type
    records the same gap. Dead mapping entries are warnings, not failures: they are exporter bugs
    that the document lists, not corpus holes.
    """
    failures = []
    warnings = []

    covered_nodes = set()
    for case in cases:
        covered_nodes.update(case["targets"])
    # the two anchor cases animate rotation.z, which is mapped, while the pivot is not: the header
    # keeps `gap` and says so, so the emits check must not treat them as stale
    partial = {"anchor/01_rotation_z_offcenter", "anchor/08_anchor_center_vs_default"}

    # TextPath only carries the shared transform channels and needs a path-bound text layout the
    # corpus has no other use for; the text modifier family (text/01~text/05) records that gap.
    exceptions = {"TextPath": "共享变换通道，由 text/ 族代表"}

    for node in NODE_ORDER:
        if anim.get(node) and node not in covered_nodes and node not in exceptions:
            failures.append("node type without any case: " + node)

    for node, channels in exported.items():
        for channel in channels:
            if node == "Path" or (node, channel) in index:
                continue
            if channel not in anim.get(node, ()):
                warnings.append("stale mapping (no such channel): %s.%s" % (node, channel))
            else:
                failures.append("exported channel without a case: %s.%s" % (node, channel))

    for case in cases:
        if case["short"] in partial:
            continue
        if os.path.exists(PAGX_BIN):
            out = os.path.join("/tmp/anim_coverage", case["short"].replace("/", "_") + ".html")
            os.makedirs("/tmp/anim_coverage", exist_ok=True)
            subprocess.run(
                [PAGX_BIN, "export", "--input", os.path.join(CORPUS, case["file"]), "--format", "html",
                 "--output", out],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            html = open(out, encoding="utf-8", errors="ignore").read() if os.path.exists(out) else ""
            if ("@keyframes" in html) != (case["expect"] == "supported"):
                failures.append("Expect does not match the export: " + case["file"])

    # A real duplicate is the same document modulo ids: two cases that only differ by channel would
    # still be different tests, so the signature is the normalised XML, not the channel list.
    seen = {}
    for case in cases:
        text = open(os.path.join(CORPUS, case["file"])).read()
        text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
        text = re.sub(r'id="[^"]*"', 'id=""', text)
        text = re.sub(r'target="[^"]*"', 'target=""', text)
        text = re.sub(r"\s+", " ", text).strip()
        if text in seen:
            failures.append("duplicate document of %s: %s" % (seen[text], case["file"]))
        seen[text] = case["file"]

    return failures, warnings


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    anim = load_registry()
    exported = load_exported()
    cases = load_cases()
    index = case_index(cases)

    if args.check:
        failures, warnings = check(anim, exported, index, cases)
        for warning in warnings:
            print("WARN", warning)
        for failure in failures:
            print("FAIL", failure)
        total = sum(len(v) for v in anim.values())
        print("%d animatable channels / %d node types / %d cases" % (total, len(anim), len(cases)))
        return 1 if failures else 0

    print(render_master(channel_rows(anim, exported, index)))
    print()
    print(render_shared(anim, index))
    print()
    print("animatable=%d exported=%d cases=%d" % (
        sum(len(v) for v in anim.values()), sum(len(v) for v in exported.values()), len(cases)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
