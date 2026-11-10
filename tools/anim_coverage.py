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
# Every exemption is one of four kinds. The list no longer means "this can never work": it says why
# the CSS pass cannot do it and who picks it up next.
#   js-pending       the CSS exporter cannot express it; a JS export can redraw it per frame
#   js-after-static  the static export does not draw the effect yet, so animation has nothing to move
#   renderer-gap     the runtime never applies the channel, so no exporter (CSS or JS) can show it
#   no-op            the value cannot change a rendered pixel; nothing to implement
# A channel listed here still keeps its `Expect: gap` case whenever one exists, so the gap stays
# measured instead of disappearing from the table.
JS_PENDING = "js-pending：CSS 无法表达，JS 逐帧重绘"
JS_AFTER_STATIC = "js-after-static：静态尚未画出该效果，补齐后再逐帧动画"
RENDERER_GAP = "renderer-gap：渲染器不应用该通道，参考帧不动，导出侧（含 JS）无解"
NO_OP = "no-op：合成顺序决定它不改变画面，无需实现"

CHANNEL_EXEMPTIONS = {
    # Scaling happens before the rotation on a z=0 plane, so scale.z cannot reach a pixel.
    ("Layer", "scale.z"): NO_OP + "（P·T·R·(K·S)，见 README §7）",
}

# Whole nodes whose static export is missing or rasterised. Drop the entry once the node is drawn.
NODE_EXEMPTIONS = {
    # A CSS conic-gradient inside pattern/foreignObject does not paint reliably in headless Chromium,
    # so the fill is emitted as a PNG and there is no CSS gradient left to move.
    "ConicGradient": JS_PENDING + "（静态已光栅化成 <image>，逐帧重绘位图即可）",
    "DiamondGradient": JS_PENDING + "（同上）",
    "NoiseStyle": JS_AFTER_STATIC + "（writer 零引用，t=0 画面为空）",
    "NoiseFilter": JS_AFTER_STATIC + "（t=0 只有底色、没有噪点）",
}

# A pipeline filter's geometry (dx / dy / stdDeviation) and its shadowOnly switch are attributes on
# the SVG filter primitives, not CSS properties: measured, keyframes that set dx / stdDeviation are
# ignored (zero pixels change between the first and last frame), while flood-color does animate.
for _channel in ("blurX", "blurY", "offsetX", "offsetY", "shadowOnly"):
    CHANNEL_EXEMPTIONS[("InnerShadowFilter", _channel)] = JS_PENDING + "（重写 <filter> 原语）"
CHANNEL_EXEMPTIONS[("DropShadowFilter", "shadowOnly")] = JS_PENDING + "（重写 <filter> 原语）"

# Text content is not an animatable property: changing the string re-shapes and re-lays out the
# line, which only a JS export can do (CSS can replace content of a pseudo element, not the run).
CHANNEL_EXEMPTIONS[("Text", "text")] = JS_PENDING + "（重新 shaping 与排版）"

# A colour stop's position is the `offset` attribute of <stop>, not a CSS property.
CHANNEL_EXEMPTIONS[("ColorStop", "offset")] = JS_PENDING + "（逐帧重建 <stop> 的 offset）"

# Style parameters the CSS exporter cannot drive:
# - a single-axis blur (blurX or blurY alone) has no CSS equivalent: filter:blur() and
#   backdrop-filter:blur() are isotropic, measured 0.96 against the reference;
# - an inner shadow's offset / blur / spread live on SVG filter primitives (feOffset, feGaussianBlur).
for _channel in ("blurX", "blurY", "showBehindLayer"):
    CHANNEL_EXEMPTIONS[("DropShadowStyle", _channel)] = JS_PENDING + "（CSS blur 各向同性，实测 0.96）"
for _channel in ("blurX", "blurY", "offsetX", "offsetY", "spread"):
    CHANNEL_EXEMPTIONS[("InnerShadowStyle", _channel)] = JS_PENDING + "（内阴影是滤镜管线，几何在原语上）"
for _channel in ("blurX", "blurY"):
    CHANNEL_EXEMPTIONS[("BackgroundBlurStyle", _channel)] = JS_PENDING + "（backdrop 模糊无单轴写法）"
# The glass effect has no CSS counterpart at all; the static export only approximates it (t=0 ~0.97),
# so it has to be drawn properly before any frame can move.
for _channel in ("depth", "dispersion", "frost", "lightAngle", "lightIntensity", "refraction", "splay"):
    CHANNEL_EXEMPTIONS[("GlassStyle", _channel)] = JS_AFTER_STATIC + "（静态只是近似，t=0 ≈0.97）"

# TextPath channels are registered as animatable in PAGXNodeChannel.cpp, but the renderer never
# applies them: LayerBuilder::convertTextPath() registers no channel writer (convertTextModifier()
# does, via setAccessor), so `pagx render --time` draws the same frame at every time.
for _channel in ("alpha", "anchor.x", "anchor.y", "position.x", "position.y", "rotation", "scale.x",
                 "scale.y", "skew", "skewAxis"):
    CHANNEL_EXEMPTIONS[("TextPath", _channel)] = RENDERER_GAP + "（convertTextPath 无 writer）"


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
