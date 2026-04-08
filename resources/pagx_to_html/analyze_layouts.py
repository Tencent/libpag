#!/usr/bin/env python3
"""Analyze all .pagx files for layout issues: elements exceeding canvas, insufficient padding."""
import xml.etree.ElementTree as ET
import os
import glob
import re
import math
import json

PAGX_DIR = os.path.dirname(os.path.abspath(__file__))
MIN_PADDING = 10

MATRIX_TRANSFORM_FILES = {
    "layer_transform_matrix.pagx",
    "layer_transform_matrix3d.pagx",
    "layer_transform_priority.pagx",
}


def parse_position(pos_str):
    if not pos_str:
        return None
    parts = pos_str.split(",")
    return float(parts[0]), float(parts[1])


def parse_size(size_str):
    if not size_str:
        return None
    parts = size_str.split(",")
    return float(parts[0]), float(parts[1])


def get_stroke_expansion(layer_elem):
    max_exp = 0
    for stroke in layer_elem.findall("Stroke"):
        width = float(stroke.get("width", "1"))
        align = stroke.get("align", "center")
        if align == "center":
            max_exp = max(max_exp, width / 2)
        elif align == "outside":
            max_exp = max(max_exp, width)
    return max_exp


def get_shadow_expansion(layer_elem):
    left = right = top = bottom = 0
    for shadow in layer_elem.findall("DropShadowStyle"):
        ox = float(shadow.get("offsetX", "0"))
        oy = float(shadow.get("offsetY", "0"))
        bx = float(shadow.get("blurX", "0"))
        by = float(shadow.get("blurY", "0"))
        blur_rx = bx * 1.5
        blur_ry = by * 1.5
        left = max(left, blur_rx - ox)
        right = max(right, blur_rx + ox)
        top = max(top, blur_ry - oy)
        bottom = max(bottom, blur_ry + oy)
    return left, top, right, bottom


def get_text_bounds(elem, layer_x=0, layer_y=0):
    pos = parse_position(elem.get("position"))
    if pos is None:
        return None
    tx, ty = pos[0] + layer_x, pos[1] + layer_y
    font_size = float(elem.get("fontSize", "12"))
    anchor = elem.get("textAnchor", "start")
    text = elem.get("text", "")
    char_width = font_size * 0.6
    letter_spacing = float(elem.get("letterSpacing", "0"))
    text_width = len(text) * (char_width + letter_spacing)
    ascent = font_size * 0.8
    descent = font_size * 0.25
    if anchor == "center":
        minx = tx - text_width / 2
        maxx = tx + text_width / 2
    elif anchor == "end":
        minx = tx - text_width
        maxx = tx
    else:
        minx = tx
        maxx = tx + text_width
    return [minx, ty - ascent, maxx, ty + descent]


def get_element_bounds(elem, layer_x=0, layer_y=0):
    tag = elem.tag
    if tag == "Text":
        return get_text_bounds(elem, layer_x, layer_y)
    pos = parse_position(elem.get("position"))
    if tag == "Rectangle":
        if pos is None:
            return None
        size = parse_size(elem.get("size"))
        if size is None:
            return None
        cx, cy = pos[0] + layer_x, pos[1] + layer_y
        hw, hh = size[0] / 2, size[1] / 2
        return [cx - hw, cy - hh, cx + hw, cy + hh]
    elif tag == "Ellipse":
        if pos is None:
            return None
        size = parse_size(elem.get("size"))
        if size is None:
            return None
        cx, cy = pos[0] + layer_x, pos[1] + layer_y
        hw, hh = size[0] / 2, size[1] / 2
        return [cx - hw, cy - hh, cx + hw, cy + hh]
    elif tag == "Polystar":
        if pos is None:
            return None
        cx, cy = pos[0] + layer_x, pos[1] + layer_y
        outer_r = float(elem.get("outerRadius", "0"))
        return [cx - outer_r, cy - outer_r, cx + outer_r, cy + outer_r]
    elif tag == "Path":
        data = elem.get("data", "")
        if not data:
            return None
        numbers = re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)', data)
        if len(numbers) < 2:
            return None
        coords = [float(n) for n in numbers]
        xs = [coords[i] + layer_x for i in range(0, len(coords), 2) if i + 1 < len(coords)]
        ys = [coords[i + 1] + layer_y for i in range(0, len(coords), 2) if i + 1 < len(coords)]
        if not xs or not ys:
            return None
        return [min(xs), min(ys), max(xs), max(ys)]
    return None


def union_bounds(a, b):
    if a is None:
        return b
    if b is None:
        return a
    return [min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3])]


def get_group_bounds(group_elem, layer_x=0, layer_y=0):
    group_pos = parse_position(group_elem.get("position"))
    gx = (group_pos[0] if group_pos else 0) + layer_x
    gy = (group_pos[1] if group_pos else 0) + layer_y
    bounds = None
    for child in group_elem:
        if child.tag in ("Rectangle", "Ellipse", "Polystar", "Text", "Path"):
            eb = get_element_bounds(child, gx, gy)
            if eb:
                bounds = union_bounds(bounds, eb)
        elif child.tag == "Group":
            gb = get_group_bounds(child, gx, gy)
            if gb:
                bounds = union_bounds(bounds, gb)
    return bounds


def rotate_point(px, py, ax, ay, angle_deg):
    """Rotate point (px,py) around anchor (ax,ay) by angle_deg degrees."""
    rad = math.radians(angle_deg)
    cos_a = math.cos(rad)
    sin_a = math.sin(rad)
    dx = px - ax
    dy = py - ay
    return ax + dx * cos_a - dy * sin_a, ay + dx * sin_a + dy * cos_a


def get_repeater_bounds(base_bounds, layer):
    rep = layer.find("Repeater")
    if rep is None or base_bounds is None:
        return base_bounds
    copies = float(rep.get("copies", "1"))
    if copies <= 0:
        return None
    rep_pos = parse_position(rep.get("position"))
    rep_rotation = float(rep.get("rotation", "0"))
    rep_anchor = parse_position(rep.get("anchor"))
    rep_offset = float(rep.get("offset", "0"))
    rep_scale = parse_position(rep.get("scale"))
    layer_x = float(layer.get("x", "0"))
    layer_y = float(layer.get("y", "0"))

    corners = [
        (base_bounds[0], base_bounds[1]),
        (base_bounds[2], base_bounds[1]),
        (base_bounds[0], base_bounds[3]),
        (base_bounds[2], base_bounds[3]),
    ]

    bounds = list(base_bounds)
    int_copies = int(math.ceil(copies))

    for i in range(1, int_copies + 1):
        idx = rep_offset + i
        dx = rep_pos[0] * idx if rep_pos else 0
        dy = rep_pos[1] * idx if rep_pos else 0
        angle = rep_rotation * idx

        if abs(angle) > 0.01 and rep_anchor:
            ax = rep_anchor[0] + layer_x
            ay = rep_anchor[1] + layer_y
            for cx, cy in corners:
                rx, ry = rotate_point(cx + dx, cy + dy, ax, ay, angle)
                bounds = union_bounds(bounds, [rx, ry, rx, ry])
        else:
            copy_bounds = [
                base_bounds[0] + dx, base_bounds[1] + dy,
                base_bounds[2] + dx, base_bounds[3] + dy,
            ]
            bounds = union_bounds(bounds, copy_bounds)

    return bounds


def analyze_layer(layer, resources_paths=None):
    if layer.get("visible") == "false":
        return None
    if layer.get("scrollRect"):
        return None
    if layer.get("matrix") or layer.get("matrix3d"):
        return None
    if layer.get("composition"):
        return None

    layer_x = float(layer.get("x", "0"))
    layer_y = float(layer.get("y", "0"))
    stroke_exp = get_stroke_expansion(layer)
    shadow_l, shadow_t, shadow_r, shadow_b = get_shadow_expansion(layer)

    has_textbox = layer.find("TextBox") is not None
    has_textpath = layer.find("TextPath") is not None
    bounds = None

    if has_textbox:
        textbox = layer.find("TextBox")
        pos = parse_position(textbox.get("position"))
        size = parse_size(textbox.get("size"))
        if pos and size and (size[0] > 0 or size[1] > 0):
            bx, by = pos[0] + layer_x, pos[1] + layer_y
            bounds = [bx, by, bx + size[0], by + size[1]]
        for child in layer:
            if child.tag in ("Rectangle", "Ellipse", "Polystar", "Path"):
                eb = get_element_bounds(child, layer_x, layer_y)
                if eb:
                    bounds = union_bounds(bounds, eb)
    elif has_textpath:
        textpath = layer.find("TextPath")
        path_data = textpath.get("path", "")
        # Resolve resource references
        resolved = None
        if path_data.startswith("@") and resources_paths:
            ref_id = path_data[1:]
            resolved = resources_paths.get(ref_id)
        elif not path_data.startswith("@"):
            resolved = path_data
        if resolved:
            numbers = re.findall(r'[-+]?(?:\d+\.?\d*|\.\d+)', resolved)
            if len(numbers) >= 2:
                coords = [float(n) for n in numbers]
                xs = [coords[i] + layer_x for i in range(0, len(coords), 2) if i + 1 < len(coords)]
                ys = [coords[i + 1] + layer_y for i in range(0, len(coords), 2) if i + 1 < len(coords)]
                if xs and ys:
                    text_elem = layer.find("Text")
                    fs = float(text_elem.get("fontSize", "12")) if text_elem is not None else 12
                    bounds = [min(xs) - fs, min(ys) - fs, max(xs) + fs, max(ys) + fs]
    else:
        for child in layer:
            if child.tag in ("Rectangle", "Ellipse", "Polystar", "Text", "Path"):
                eb = get_element_bounds(child, layer_x, layer_y)
                if eb:
                    bounds = union_bounds(bounds, eb)
            elif child.tag == "Group":
                gb = get_group_bounds(child, layer_x, layer_y)
                if gb:
                    bounds = union_bounds(bounds, gb)

    if bounds is None:
        return None

    if layer.find("Repeater") is not None:
        bounds = get_repeater_bounds(bounds, layer)
        if bounds is None:
            return None

    if stroke_exp > 0:
        bounds[0] -= stroke_exp
        bounds[1] -= stroke_exp
        bounds[2] += stroke_exp
        bounds[3] += stroke_exp

    bounds[0] -= shadow_l
    bounds[1] -= shadow_t
    bounds[2] += shadow_r
    bounds[3] += shadow_b

    return bounds


def is_bg_rect(layer, canvas_w, canvas_h):
    rect = layer.find("Rectangle")
    if rect is None:
        return False
    rsize = parse_size(rect.get("size"))
    rpos = parse_position(rect.get("position"))
    if rsize and rpos:
        return abs(rsize[0] - canvas_w) < 1 and abs(rsize[1] - canvas_h) < 1
    return False


def get_resources_paths(root):
    """Extract PathData resources from <Resources> section."""
    paths = {}
    resources = root.find("Resources")
    if resources is not None:
        for pd in resources.findall("PathData"):
            pid = pd.get("id")
            data = pd.get("data")
            if pid and data:
                paths[pid] = data
    return paths


def analyze_file(filepath):
    filename = os.path.basename(filepath)
    if filename in MATRIX_TRANSFORM_FILES:
        return None

    tree = ET.parse(filepath)
    root = tree.getroot()
    canvas_w = float(root.get("width", "0"))
    canvas_h = float(root.get("height", "0"))
    if canvas_w == 0 or canvas_h == 0:
        return None

    resources_paths = get_resources_paths(root)
    all_bounds = None
    overflow_layers = []
    has_bg = False
    layers = list(root.findall("Layer"))

    for i, layer in enumerate(layers):
        if layer.get("visible") == "false":
            continue
        if i == 0 and is_bg_rect(layer, canvas_w, canvas_h):
            has_bg = True
            continue
        bounds = analyze_layer(layer, resources_paths)
        if bounds:
            all_bounds = union_bounds(all_bounds, bounds)
            if bounds[0] < 0 or bounds[1] < 0 or bounds[2] > canvas_w or bounds[3] > canvas_h:
                overflow_layers.append({
                    "layer_index": i,
                    "bounds": [round(b, 1) for b in bounds],
                    "overflow": {
                        "left": round(max(0, -bounds[0]), 1),
                        "top": round(max(0, -bounds[1]), 1),
                        "right": round(max(0, bounds[2] - canvas_w), 1),
                        "bottom": round(max(0, bounds[3] - canvas_h), 1),
                    }
                })

    if all_bounds is None:
        return None

    needed_left = max(0, MIN_PADDING - all_bounds[0])
    needed_top = max(0, MIN_PADDING - all_bounds[1])
    needed_right = max(0, all_bounds[2] - (canvas_w - MIN_PADDING))
    needed_bottom = max(0, all_bounds[3] - (canvas_h - MIN_PADDING))

    needed_left = math.ceil(needed_left / 2) * 2
    needed_top = math.ceil(needed_top / 2) * 2
    needed_right = math.ceil(needed_right / 2) * 2
    needed_bottom = math.ceil(needed_bottom / 2) * 2

    if needed_left == 0 and needed_top == 0 and needed_right == 0 and needed_bottom == 0:
        return None

    new_w = int(canvas_w + needed_left + needed_right)
    new_h = int(canvas_h + needed_top + needed_bottom)

    return {
        "filename": filename,
        "filepath": filepath,
        "canvas_w": int(canvas_w),
        "canvas_h": int(canvas_h),
        "content_bounds": [round(b, 1) for b in all_bounds],
        "new_w": new_w,
        "new_h": new_h,
        "offset_x": needed_left,
        "offset_y": needed_top,
        "expand_right": needed_right,
        "expand_bottom": needed_bottom,
        "has_bg_rect": has_bg,
        "overflow_layers": overflow_layers,
    }


def main():
    pagx_files = sorted(glob.glob(os.path.join(PAGX_DIR, "*.pagx")))
    print(f"Found {len(pagx_files)} .pagx files\n")

    results = []
    for f in pagx_files:
        result = analyze_file(f)
        if result:
            results.append(result)

    if not results:
        print("No files need modification!")
        return

    print(f"Files needing modification: {len(results)}\n")
    print("=" * 80)

    for r in results:
        print(f"\nFile: {r['filename']}")
        print(f"  Current:   {r['canvas_w']} x {r['canvas_h']}")
        cb = r['content_bounds']
        print(f"  Bounds:    [{cb[0]}, {cb[1]}, {cb[2]}, {cb[3]}]")
        print(f"  New:       {r['new_w']} x {r['new_h']}")
        print(f"  Expand:    L={r['offset_x']} T={r['offset_y']} R={r['expand_right']} B={r['expand_bottom']}")
        print(f"  BG rect:   {r['has_bg_rect']}")
        if r['overflow_layers']:
            for ol in r['overflow_layers'][:3]:
                o = ol['overflow']
                print(f"    OVF L{ol['layer_index']}: L={o['left']} T={o['top']} R={o['right']} B={o['bottom']}")

    print(f"\n{'='*80}")
    print(f"Summary: {len(results)} / {len(pagx_files)} files need modification")

    # Also write JSON for the batch modifier script
    json_path = os.path.join(PAGX_DIR, "_analysis_results.json")
    with open(json_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nJSON results written to {json_path}")


if __name__ == "__main__":
    main()
