#!/usr/bin/env python3
"""Derive the animation coverage tables from the PAGX channel registry, the HTML writer's channel
table and the corpus headers.

Run from the repo root:

    python3 tools/anim_coverage.py                # print the tables
    python3 tools/anim_coverage.py --check        # verify the invariants, non-zero exit on failure
    python3 tools/anim_coverage.py --emit-readme  # print the counts README.md quotes

The tables are copied into resources/pagx_to_html/animations/README.md, so a reviewer can re-run
this to see that the document still matches the code. `--check` proves that the set of animatable
channels parsed out of PAGXNodeChannel.cpp equals `supported ∪ exempt`, which is what makes the
README's completeness claim machine-checkable: deleting a registry row, adding an unknown macro
shape or registering an exemption without a reason all turn it red.
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

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

# A registry row is animatable when its fourth argument is Anim/AnimLayout. `FIELD_ENUM` appends the
# enum type as a fifth argument, so the tail must accept `, Anim, BlendMode)`: matching only
# `, Anim)` silently dropped `Layer.blendMode` from the universe.
ANIM_LINE = re.compile(r", (?:Anim|AnimLayout)[,)]")
ROW = re.compile(r'FIELD_\w+\((\w+), "([^"]+)", \w+, (?:Anim|AnimLayout)(?:, \w+)?\)')
COLOR_ROW = '{"color", Anim, &AccessFillStrokeColor}'

# `AppendGroupCommonFields<T>` writes its rows with the template parameter as the node, so each of
# those rows belongs to every node that instantiates it.
SHARED_TARGETS = ("Group", "TextBox", "TextPath")

# Nodes that own no animatable FIELD_* row by design: Path's `point{N}.x/y` sub-channels are parsed
# before the writer's table lookup, so an empty parse is expected there and only there.
NODE_WITHOUT_ROWS = {"Path"}

# Rows whose macro shape differs from the common one, anchored literally: they are the ones a
# refactor can delete or de-animate without changing any other line, and the universe would shrink
# silently because no other rule looks at them.
REQUIRED_ROWS = (
    'FIELD_MATRIX3D(Layer, "matrix3D"',
    'FIELD_ENUM(Layer, "blendMode"',
)

# The parse floor: the registry has grown monotonically, so a total below this means the parse
# broke rather than the feature set shrinking. Raise it when channel families are added.
UNIVERSE_FLOOR = 150

# Case headers that drive two nodes write two `Target:` lines, and case_pairs() flattens a header into
# one cross product, so these pairs appear in the index without any fixture claiming them
# (`geometry/05` drives Rectangle.size.width and Fill.color; `stroke/06` drives TrimPath.end and
# Stroke.width). They are listed instead of being special-cased per file so that the reverse
# assertion -- a case must not drive a channel the registry does not declare -- still has teeth.
CROSS_NODE_PAIRS = {
    ("Rectangle", "color"),
    ("Fill", "size.width"),
    ("TrimPath", "width"),
    ("Stroke", "end"),
}


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
# Not a channel but a combination: `TrimPath` on a filled shape trims the path itself, so the filled
# area shrinks with the window (measured 178px -> 130px on one rectangle). CSS can only dash the
# stroke, so the fill part waits for the JS pass while the stroke part is supported.
JS_PENDING = "js-pending：CSS 无法表达，JS 逐帧重绘"
JS_AFTER_STATIC = "js-after-static：静态尚未画出该效果，补齐后再逐帧动画"
RENDERER_GAP = "renderer-gap：渲染器不应用该通道，参考帧不动，导出侧（含 JS）无解"
NO_OP = "no-op：合成顺序决定它不改变画面，无需实现"
EXEMPTION_TAGS = ("js-pending", "js-after-static", "renderer-gap", "no-op")

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

# A blur along one axis only: filter:blur() is isotropic, so a blurX-only or blurY-only animation
# cannot be reproduced in CSS. Measured 0.96-0.97 against the reference, the same ceiling the style
# family's single-axis blur hits, so it joins the JS pass.
for _channel in ("blurX", "blurY"):
    CHANNEL_EXEMPTIONS[("BlurFilter", _channel)] = JS_PENDING + "（CSS 模糊各向同性，只能做两轴同值）"

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
    """Returns (node -> {channel}, audit).

    `audit` keeps the provenance check_registry() needs to prove the parse lost nothing:
    `macro_lines` is every line whose fourth argument is Anim/AnimLayout (the universe the source
    declares), `consumed` the lines a rule below turned into a binding, `shared_lines` the rows of
    `AppendGroupCommonFields<T>` (each expands to one binding per SHARED_TARGETS entry), `specials`
    the `{"color", Anim, ...}` rows written without a FIELD_* macro, and `missing` the anchors that
    were expected but not found.
    """
    text = open(CHANNEL_SRC).read()
    lines = text.split("\n")
    audit = {
        "lines": lines,
        "macro_lines": [],
        "consumed": set(),
        "shared_lines": [],
        "specials": [],
        "missing": [],
    }
    for index, line in enumerate(lines):
        if ANIM_LINE.search(line):
            audit["macro_lines"].append(index + 1)
    for anchor in REQUIRED_ROWS:
        if anchor not in text:
            audit["missing"].append("required registry row is gone: %s" % anchor)

    # Walking lines rather than split() bodies keeps the line number of every binding, which is what
    # makes "this row was never consumed" reportable. `in_shared` follows the one function whose rows
    # are written with `T`; `func_name` is the enclosing `Build*` builder, or None outside one.
    in_shared = False
    func_name = None
    bindings = []  # (node or None for shared, channel, line number, kind)
    for index, line in enumerate(lines):
        lineno = index + 1
        stripped = line.strip()
        if stripped.startswith("static void AppendGroupCommonFields"):
            in_shared = True
        elif stripped.startswith("static std::vector<ChannelDef> "):
            in_shared = False
            func_name = stripped[len("static std::vector<ChannelDef> "):].split("(")[0].strip()
        elif line.startswith("}"):
            # Only an unindented closing brace ends a function: an indented `};` closes a nested
            # initializer and must not reset the builder the following rows belong to.
            in_shared = False
            func_name = None
            continue
        row = ROW.search(line)
        if row:
            node, channel = row.group(1), row.group(2)
            if in_shared:
                bindings.append((None, channel, lineno, "shared"))
            elif func_name is None or node == "T":
                # The `T` rows outside `AppendGroupCommonFields<T>` name no node type, and a row
                # outside a builder is in no table at all: neither is consumed, so check_registry()
                # reports the line instead of inventing a binding for it.
                continue
            else:
                bindings.append((node, channel, lineno, "row"))
            audit["consumed"].add(lineno)
        elif COLOR_ROW in line:
            node = {"BuildFillFields": "Fill", "BuildStrokeFields": "Stroke"}.get(func_name)
            if node is not None:
                bindings.append((node, "color", lineno, "special"))
                audit["consumed"].add(lineno)

    anim = {}
    for node, channel, lineno, kind in bindings:
        if kind == "shared":
            audit["shared_lines"].append(lineno)
            targets = SHARED_TARGETS
        else:
            if kind == "special":
                audit["specials"].append(lineno)
            targets = (node,)
        for target in targets:
            anim.setdefault(target, set()).add(channel)
    return anim, audit


def load_exported():
    src = open(WRITER_SRC).read()
    table = re.search(r"kChannelBindingTable\[\] = \{(.*?)\n\};", src, re.S).group(1)
    exported = {}
    for m in re.finditer(r'\{NodeType::(\w+), "([^"]+)"', table):
        exported.setdefault(m.group(1), set()).add(m.group(2))
    # `Path.point{N}.x/y` is parsed before the table lookup
    exported.setdefault("Path", set()).add("point{N}.x/y")
    return exported


def case_pairs(header):
    """(node, channel) pairs a fixture header claims.

    The header is read as one target list times one channel list. A fixture that drives two nodes
    writes two `Target:` lines, so that product also yields pairs nobody claims
    (`geometry/05` gives (Fill, size.width) as well as the real (Fill, color)); those are listed in
    CROSS_NODE_PAIRS rather than silently renamed, because the product only ever over-credits and
    the pair list is used to catch a registry row whose case outlived it.
    """
    targets = re.findall(r"Target:\s*(\S+)", header)
    channels = []
    for group in re.findall(r"Channel:\s*(\S+)", header):
        channels.extend(part.strip() for part in group.split(",") if part.strip())
    return [(target, channel) for target in targets for channel in channels]


def load_cases():
    cases = []
    for path in sorted(glob.glob(os.path.join(CORPUS, "**", "*.pagx"), recursive=True)):
        text = open(path).read()
        rel = os.path.relpath(path, CORPUS).replace(os.sep, "/")
        header = text[: text.find("-->")]
        pairs = case_pairs(header)
        cases.append(
            {
                "file": rel,
                "short": rel[:-5] if rel.endswith(".pagx") else rel,
                "family": rel.split("/")[0] if "/" in rel else "(root)",
                "targets": list(dict.fromkeys(node for node, _ in pairs)),
                "channels": list(dict.fromkeys(channel for _, channel in pairs)),
                "pairs": pairs,
            }
        )
    return cases


def case_index(cases):
    """(node, channel) -> [case short names]"""
    index = {}
    for case in cases:
        for pair in case["pairs"]:
            index.setdefault(pair, []).append(case["short"])
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


def check_well_formed(cases):
    """Every fixture must parse: a stray `-->`-eating edit breaks the document silently, and no
    other gate here reads the body (the header regexes stop at the first line)."""
    failures = []
    for case in cases:
        path = os.path.join(CORPUS, case["file"])
        try:
            ET.parse(str(path))
        except ET.ParseError as error:
            failures.append("fixture is not well-formed XML: %s (%s)" % (case["file"], error))
    return failures


def universe(anim):
    return set((node, channel) for node, channels in anim.items() for channel in channels)


def exempt_set(anim):
    """Exempt channels: the per-channel table plus every channel of an exempt node."""
    exempt = set(CHANNEL_EXEMPTIONS)
    for node in NODE_EXEMPTIONS:
        for channel in anim.get(node, ()):
            exempt.add((node, channel))
    return exempt


def check_reason(label, reason):
    if not reason or not reason.strip():
        return ["exemption has no reason: " + label]
    tag = reason.split("：")[0].strip()
    if tag not in EXEMPTION_TAGS:
        return ["exemption reason must start with one of %s: %s (%s)"
                % ("/".join(EXEMPTION_TAGS), label, reason)]
    return []


def check_registry(anim, audit):
    """Proves that the parsed universe equals `supported ∪ exempt` and that the parse saw every
    animatable row the registry declares.

    The registry file is the source of truth: ANIM_LINE counts the rows it declares, and a row no
    rule consumed means the parse dropped a feature without changing any other signal. Four other
    shapes shrink the universe silently: a node that parses to nothing, an exemption that names a
    channel nobody declares (it hides nothing and proves nothing), a macro row deleted outright, and
    a parse that only looks at a fraction of the file. `--check` would otherwise claim completeness
    over a subset it invented.
    """
    failures = []
    lines = audit["lines"]
    macro_lines = audit["macro_lines"]
    for lineno in sorted(set(macro_lines) - audit["consumed"]):
        failures.append(
            "registry row not consumed by any rule: %s:%d  %s"
            % (os.path.basename(CHANNEL_SRC), lineno, lines[lineno - 1].strip())
        )

    parsed = universe(anim)
    expected = len(macro_lines) + len(audit["shared_lines"]) * (len(SHARED_TARGETS) - 1)
    if expected != len(parsed):
        failures.append(
            "registry line count does not match the parse: %d macro rows + %d shared rows x %d "
            "targets = %d channels, parsed %d"
            % (len(macro_lines), len(audit["shared_lines"]), len(SHARED_TARGETS), expected, len(parsed))
        )
    failures.extend(audit["missing"])

    if not parsed:
        failures.append("registry parsed to zero animatable channels")
    if len(parsed) < UNIVERSE_FLOOR:
        failures.append(
            "parsed %d channels, below the floor %d: the parse or the registry shrank"
            % (len(parsed), UNIVERSE_FLOOR)
        )
    for node in NODE_ORDER:
        if node in NODE_EXEMPTIONS or node in NODE_WITHOUT_ROWS:
            continue
        if not anim.get(node):
            failures.append("node parsed to zero channels: %s" % node)

    for node, channel in sorted(CHANNEL_EXEMPTIONS):
        label = "%s.%s" % (node, channel)
        failures.extend(check_reason("channel exemption " + label, CHANNEL_EXEMPTIONS[(node, channel)]))
        if (node, channel) not in parsed:
            failures.append("channel exemption for a channel the registry does not declare: " + label)
    for node in sorted(NODE_EXEMPTIONS):
        failures.extend(check_reason("node exemption " + node, NODE_EXEMPTIONS[node]))
        if not anim.get(node):
            failures.append("node exemption for a node the registry does not declare: " + node)

    exempt = exempt_set(anim)
    supported = parsed - exempt
    # `supported` is derived by subtraction, so the first direction is structural and the second is
    # the one that fires in practice: an exemption naming a channel outside the registry.
    for node, channel in sorted(parsed - (supported | exempt)):
        failures.append("channel in neither supported nor exempt: %s.%s" % (node, channel))
    for node, channel in sorted((supported | exempt) - parsed):
        failures.append("channel outside the registry universe: %s.%s" % (node, channel))
    return failures


def check_case_pairs(anim, cases):
    """Every channel a single-node case drives must exist in the registry.

    This is the other half of "no registry row is lost": deleting a row leaves its case driving a
    channel nothing declares any more, and the universe would shrink without any count changing.
    Composite cases list several nodes' channels under one header and are skipped. `Path.point{N}.x/y`
    is the one documented exception: the writer parses it before its table lookup, so no FIELD_* row
    exists for it.
    """
    failures = []
    parsed = universe(anim)
    for case in cases:
        if case["family"] == "composite":
            continue
        for node, channel in case["pairs"]:
            if (node, channel) in parsed or (node, channel) in CROSS_NODE_PAIRS:
                continue
            if node == "Path" and re.match(r"point\d+\.[xy]$", channel):
                continue
            failures.append(
                "case drives a channel the registry does not declare: %s.%s (%s)"
                % (node, channel, case["file"])
            )
    return failures


def coverage_counts(anim, audit, cases):
    parsed = universe(anim)
    exempt = exempt_set(anim)
    per_tag = dict.fromkeys(EXEMPTION_TAGS, 0)
    for reason in CHANNEL_EXEMPTIONS.values():
        tag = reason.split("：")[0].strip()
        if tag in per_tag:
            per_tag[tag] += 1
    for node, reason in NODE_EXEMPTIONS.items():
        tag = reason.split("：")[0].strip()
        if tag in per_tag:
            per_tag[tag] += len(anim.get(node, ()))
    return {
        "macro_lines": len(audit["macro_lines"]),
        "shared_lines": len(audit["shared_lines"]),
        "shared_targets": len(SHARED_TARGETS),
        "universe": len(parsed),
        "supported": len(parsed - exempt),
        "exempt": len(exempt),
        "per_tag": per_tag,
        "cases": len(cases),
    }


def render_readme(counts):
    """The numbers README.md quotes: printed rather than typed, so the document cannot drift."""
    shared = counts["shared_lines"]
    lines = [
        "<!-- generated by `python3 tools/anim_coverage.py --emit-readme`; do not edit by hand -->",
        "",
        "| 量 | 值 |",
        "|----|----|",
        "| 宏表可动画行（`, Anim` / `, AnimLayout`） | %d |" % counts["macro_lines"],
        "| 其中 `AppendGroupCommonFields` 公共行（×%d 节点） | %d |"
        % (counts["shared_targets"], shared),
        "| 行数对账（%d + %d×%d） | %d |"
        % (counts["macro_lines"], shared, counts["shared_targets"] - 1, counts["universe"]),
        "| 通道全集 | %d |" % counts["universe"],
        "| supported（全集 − 豁免） | %d |" % counts["supported"],
        "| exempt（登记在 `tools/anim_coverage.py`） | %d |" % counts["exempt"],
    ]
    tags = list(EXEMPTION_TAGS)
    for index, tag in enumerate(tags):
        branch = "└" if index == len(tags) - 1 else "├"
        lines.append("| %s `%s` | %d |" % (branch, tag, counts["per_tag"][tag]))
    lines.append("| 用例数 | %d |" % counts["cases"])
    lines.append("")
    lines.append(
        "%d = %d (supported) + %d (exempt)"
        % (counts["universe"], counts["supported"], counts["exempt"])
    )
    return "\n".join(lines)


def check(anim, exported, index, cases, audit):
    """Returns (failures, warnings).

    Every animatable channel in the PAGX registry needs a case that drives it, whether the exporter
    supports it (`Expect: supported`) or not yet (`Expect: gap`). The corpus is the inventory of
    PAGX animation features, not of the exporter's progress, so an unexported channel still needs
    its gap case: that is what turns "not implemented" into a visible, measurable item. Dead
    mapping entries are warnings, not failures: they are exporter bugs that the document lists,
    not corpus holes.
    """
    failures = check_registry(anim, audit)
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

    failures.extend(check_well_formed(cases))
    failures.extend(check_case_pairs(anim, cases))

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
        if case["family"] == "composite":
            continue
        supported.update(case["channels"])
    for channel in sorted(supported - composed):
        failures.append("supported channel missing from a composite case: " + channel)

    for node, channels in exported.items():
        for channel in channels:
            if node == "Path" or (node, channel) in index:
                continue
            # An exempt channel needs no case even when the exporter still maps it: the mapping can
            # stay useful for the exact case (a blur with both axes equal), the gap is only that CSS
            # cannot express the single-axis form.
            if (node, channel) in CHANNEL_EXEMPTIONS:
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
            # A case only exists for a channel the exporter claims, so it must produce keyframes;
            # a static export means the mapping silently stopped working.
            if "@keyframes" not in html:
                failures.append("case does not export keyframes: " + case["file"])

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
    parser.add_argument("--emit-readme", action="store_true", dest="emit_readme")
    args = parser.parse_args()

    anim, audit = load_registry()
    exported = load_exported()
    cases = load_cases()
    index = case_index(cases)

    if args.emit_readme:
        print(render_readme(coverage_counts(anim, audit, cases)))
        return 0

    if args.check:
        failures, warnings = check(anim, exported, index, cases, audit)
        for warning in warnings:
            print("WARN", warning)
        for failure in failures:
            print("FAIL", failure)
        total = len(universe(anim))
        print("%d animatable channels (%d macro rows) / %d node types / %d cases" % (
            total, len(audit["macro_lines"]), len(anim), len(cases)))
        return 1 if failures else 0

    print(render_master(channel_rows(anim, exported, index)))
    print()
    print(render_shared(anim, index))
    print()
    print("animatable=%d exported=%d cases=%d" % (
        len(universe(anim)), sum(len(v) for v in exported.values()), len(cases)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
