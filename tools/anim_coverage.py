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


# Exemptions are only for channels that can never have a meaningful case, in one of three ways:
#   1. the value cannot change a rendered pixel (`Layer.scale.z`);
#   2. the renderer never applies it, so no reference frame moves (`TextPath`);
#   3. the node has no static export at all, so the reference frame is empty (the noise nodes).
# A channel that is merely not implemented yet does NOT belong here: its `Expect: gap` case is the
# roadmap entry, and it turns into `supported` when the channel lands. Exempting it would hide the
# work item and drop it from the coverage table.
CHANNEL_EXEMPTIONS = {
    ("Layer", "scale.z"): "合成顺序 P·T·R·(K·S) 中缩放先于旋转作用于 z=0 的平面路径，见 README §7",
}

# Whole nodes with no static export: `src/pagx/html` never references them, so nothing is painted at
# t=0 and a channel case would compare a drawn reference against an empty frame. Listed as static
# gaps in the README; drop the entry once the node is exported.
# Conic and Diamond gradients are rasterised to a PNG at export time (a CSS conic-gradient inside
# pattern/foreignObject does not paint reliably in headless Chromium), so the fill is an <image>:
# there is no CSS gradient left to move. Animating them needs a per-frame raster, i.e. the JS side.
RASTER_GRADIENT = "静态导出会光栅化成 PNG，画面上只剩 <image>，CSS 无渐变可动画，需逐帧重绘位图"
NODE_EXEMPTIONS = {
    "ConicGradient": RASTER_GRADIENT,
    "DiamondGradient": RASTER_GRADIENT,
    "NoiseStyle": "静态导出未实现该节点（writer 无引用），t=0 画面为空，需先补静态能力",
    "NoiseFilter": "静态导出未实现该节点（writer 无引用），t=0 只画出底色、无噪声，需先补静态能力",
}

# A pipeline filter's geometry (dx / dy / stdDeviation) and its shadowOnly switch are attributes on
# the SVG filter primitives, not CSS properties: measured, keyframes that set dx / stdDeviation are
# ignored (zero pixels change between the first and last frame), while flood-color does animate. So
# these channels need a JS-side export that rewrites the filter per frame and stay out of the CSS
# milestone. `filter/07,08,11,12,14,15` documents the gap.
FILTER_CSS_UNSUPPORTED = "SVG 滤镜原语的几何属性不是 CSS 属性，关键帧无效，需 JS 侧逐帧重写滤镜"
for _channel in ("blurX", "blurY", "offsetX", "offsetY", "shadowOnly"):
    CHANNEL_EXEMPTIONS[("InnerShadowFilter", _channel)] = FILTER_CSS_UNSUPPORTED
CHANNEL_EXEMPTIONS[("DropShadowFilter", "shadowOnly")] = FILTER_CSS_UNSUPPORTED
# A colour stop's position is the `offset` attribute of <stop>, not a CSS property.
CHANNEL_EXEMPTIONS[("ColorStop", "offset")] = "stop 的 offset 是 XML 属性，不是 CSS 属性，无法用关键帧驱动"

# TextPath channels are registered as animatable in PAGXNodeChannel.cpp, but the renderer never
# applies them: LayerBuilder::convertTextPath() registers no channel writer (convertTextModifier()
# does, via setAccessor), so `pagx render --time` draws the same frame at every time and a case would
# compare two static images. Measured: rotation / position.x / scale.x / alpha animations all render
# identical first and last frames. Remove these entries and add cases once the renderer writes them.
TEXT_PATH_RENDERER_GAP = "渲染器 convertTextPath() 未注册通道写入，参考帧不会动，待渲染器支持后补用例"
for _channel in ("alpha", "anchor.x", "anchor.y", "position.x", "position.y", "rotation", "scale.x",
                 "scale.y", "skew", "skewAxis"):
    CHANNEL_EXEMPTIONS[("TextPath", _channel)] = TEXT_PATH_RENDERER_GAP



def load_registry():
    src = open(CHANNEL_SRC).read()
    anim = {}
    shared = re.search(r"static void AppendGroupCommonFields.*?\n}\n", src, re.S).group(0)
    shared_rows = re.findall(r'FIELD_\w+\(T, "([^"]+)", \w+, (?:Anim|AnimLayout)\)', shared)
    funcs = re.split(r"\nstatic std::vector<ChannelDef> (Build\w+)", src)
    for i in range(1, len(funcs), 2):
        name, body = funcs[i], funcs[i + 1]
        for m in re.finditer(r'FIELD_\w+\((\w+), "([^"]+)", \w+, (?:Anim|AnimLayout)\)', body):
            # `AppendGroupCommonFields<T>` writes its rows with the template parameter as the node, so
            # `T` is not a node type: those rows are attached to Group / TextBox / TextPath below.
            # Keeping them under `T` inflated the channel total by ten and hid them from --check,
            # because check() only walks NODE_ORDER.
            if m.group(1) == "T":
                continue
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
        # `Known-gap` marks a case whose channel is exported but does not match the reference for a
        # documented reason (e.g. a CSS interpolation difference), so the keyframe presence check
        # below must not flag it.
        known_gap = re.search(r"Known-gap:\s*([\w-]+)", header)
        cases.append(
            {
                "file": rel,
                "short": rel[:-5] if rel.endswith(".pagx") else rel,
                "family": rel.split("/")[0] if "/" in rel else "(root)",
                "targets": targets,
                "channels": channels,
                "expect": expect.group(1) if expect else "?",
                "known_gap": known_gap.group(1) if known_gap else "",
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

    Every animatable channel in the PAGX registry needs a case that drives it, whether the exporter
    supports it (`Expect: supported`) or not yet (`Expect: gap`). The corpus is the inventory of
    PAGX animation features, not of the exporter's progress, so an unexported channel still needs
    its gap case: that is what turns "not implemented" into a visible, measurable item. Dead
    mapping entries are warnings, not failures: they are exporter bugs that the document lists,
    not corpus holes.
    """
    failures = []
    warnings = []

    for node in NODE_ORDER:
        if node in NODE_EXEMPTIONS:
            continue
        for channel in sorted(anim.get(node, ())):
            if (node, channel) in index:
                continue
            if (node, channel) in CHANNEL_EXEMPTIONS:
                continue
            failures.append("animatable channel without a case: %s.%s" % (node, channel))

    # A node the registry produces that NODE_ORDER does not list would be skipped by every loop
    # below, which is exactly how the `T` template parameter stayed invisible.
    for node in sorted(set(anim) - set(NODE_ORDER) - set(NODE_EXEMPTIONS)):
        failures.append("node type missing from NODE_ORDER: %s" % node)

    # Every channel the exporter implements has to appear in a composite case: single-channel cases
    # prove the mapping, composites prove the channels compose. The match is by channel name, which
    # is enough to catch omissions (a channel that no composite drives) without re-declaring the node
    # type per channel here.
    composites = [c for c in cases if c["family"] == "composite"]
    composed = set()
    for case in composites:
        composed.update(case["channels"])
    supported = set()
    for case in cases:
        if case["expect"] != "supported" or case["family"] == "composite":
            continue
        supported.update(case["channels"])
    for channel in sorted(supported - composed):
        failures.append("supported channel missing from a composite case: " + channel)

    for node, channels in exported.items():
        for channel in channels:
            if node == "Path" or (node, channel) in index:
                continue
            if channel not in anim.get(node, ()):
                warnings.append("stale mapping (no such channel): %s.%s" % (node, channel))
            else:
                failures.append("exported channel without a case: %s.%s" % (node, channel))

    for case in cases:
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
            if case.get("known_gap"):
                continue
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
