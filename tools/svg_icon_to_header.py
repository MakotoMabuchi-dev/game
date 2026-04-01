#!/usr/bin/env python3

import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TOKEN_RE = re.compile(r"[MmZzLlHhVvCcSs]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?")


def cubic_bezier(p0, p1, p2, p3, steps):
    points = []
    for i in range(1, steps + 1):
        t = i / steps
        mt = 1.0 - t
        x = (mt ** 3) * p0[0] + 3 * (mt ** 2) * t * p1[0] + 3 * mt * (t ** 2) * p2[0] + (t ** 3) * p3[0]
        y = (mt ** 3) * p0[1] + 3 * (mt ** 2) * t * p1[1] + 3 * mt * (t ** 2) * p2[1] + (t ** 3) * p3[1]
        points.append((x, y))
    return points


def parse_path(d):
    tokens = TOKEN_RE.findall(d)
    i = 0
    command = None
    current = (0.0, 0.0)
    start = (0.0, 0.0)
    last_control = None
    subpaths = []
    path = []

    def read_float():
        nonlocal i
        value = float(tokens[i])
        i += 1
        return value

    def ensure_path():
        nonlocal path
        if not path:
            path = [current]

    while i < len(tokens):
        token = tokens[i]
        if re.fullmatch(r"[MmZzLlHhVvCcSs]", token):
            command = token
            i += 1
        if command is None:
            raise ValueError("path command missing")

        if command in ("M", "m"):
            x = read_float()
            y = read_float()
            current = (current[0] + x, current[1] + y) if command == "m" else (x, y)
            start = current
            if path:
                subpaths.append(path)
            path = [current]
            last_control = None
            command = "L" if command == "M" else "l"
        elif command in ("L", "l"):
            ensure_path()
            while i < len(tokens) and not re.fullmatch(r"[A-Za-z]", tokens[i]):
                x = read_float()
                y = read_float()
                current = (current[0] + x, current[1] + y) if command == "l" else (x, y)
                path.append(current)
            last_control = None
        elif command in ("H", "h"):
            ensure_path()
            while i < len(tokens) and not re.fullmatch(r"[A-Za-z]", tokens[i]):
                x = read_float()
                current = (current[0] + x, current[1]) if command == "h" else (x, current[1])
                path.append(current)
            last_control = None
        elif command in ("V", "v"):
            ensure_path()
            while i < len(tokens) and not re.fullmatch(r"[A-Za-z]", tokens[i]):
                y = read_float()
                current = (current[0], current[1] + y) if command == "v" else (current[0], y)
                path.append(current)
            last_control = None
        elif command in ("C", "c"):
            ensure_path()
            while i < len(tokens) and not re.fullmatch(r"[A-Za-z]", tokens[i]):
                x1 = read_float()
                y1 = read_float()
                x2 = read_float()
                y2 = read_float()
                x = read_float()
                y = read_float()
                if command == "c":
                    p1 = (current[0] + x1, current[1] + y1)
                    p2 = (current[0] + x2, current[1] + y2)
                    p3 = (current[0] + x, current[1] + y)
                else:
                    p1 = (x1, y1)
                    p2 = (x2, y2)
                    p3 = (x, y)
                path.extend(cubic_bezier(current, p1, p2, p3, 10))
                current = p3
                last_control = p2
        elif command in ("S", "s"):
            ensure_path()
            while i < len(tokens) and not re.fullmatch(r"[A-Za-z]", tokens[i]):
                x2 = read_float()
                y2 = read_float()
                x = read_float()
                y = read_float()
                if last_control is None:
                    p1 = current
                else:
                    p1 = (2 * current[0] - last_control[0], 2 * current[1] - last_control[1])
                if command == "s":
                    p2 = (current[0] + x2, current[1] + y2)
                    p3 = (current[0] + x, current[1] + y)
                else:
                    p2 = (x2, y2)
                    p3 = (x, y)
                path.extend(cubic_bezier(current, p1, p2, p3, 10))
                current = p3
                last_control = p2
        elif command in ("Z", "z"):
            if path and path[0] != path[-1]:
                path.append(path[0])
            subpaths.append(path)
            path = []
            current = start
            last_control = None
        else:
            raise ValueError(f"unsupported SVG path command: {command}")

    if path:
        subpaths.append(path)
    return subpaths


def point_in_polygon(x, y, polygon):
    inside = False
    j = len(polygon) - 1

    for i in range(len(polygon)):
        xi, yi = polygon[i]
        xj, yj = polygon[j]
        intersects = ((yi > y) != (yj > y))
        if intersects:
            cross_x = ((xj - xi) * (y - yi) / (yj - yi)) + xi
            if x < cross_x:
                inside = not inside
        j = i

    return inside


def render_svg(svg_path, size):
    root = ET.parse(svg_path).getroot()
    view_box = root.attrib.get("viewBox", "0 0 24 24").split()
    min_x, min_y, width, height = [float(v) for v in view_box]
    upscale = size * 4
    mask = [[0 for _ in range(upscale)] for _ in range(upscale)]

    for path_node in root.findall(".//{http://www.w3.org/2000/svg}path"):
        if path_node.attrib.get("fill") == "none":
            continue
        for subpath in parse_path(path_node.attrib["d"]):
            scaled = []
            for x, y in subpath:
                px = ((x - min_x) / width) * (upscale - 1)
                py = ((y - min_y) / height) * (upscale - 1)
                scaled.append((px, py))
            if len(scaled) < 3:
                continue

            min_px = max(0, int(math.floor(min(x for x, _ in scaled))))
            max_px = min(upscale - 1, int(math.ceil(max(x for x, _ in scaled))))
            min_py = max(0, int(math.floor(min(y for _, y in scaled))))
            max_py = min(upscale - 1, int(math.ceil(max(y for _, y in scaled))))

            for py in range(min_py, max_py + 1):
                sample_y = py + 0.5
                for px in range(min_px, max_px + 1):
                    if point_in_polygon(px + 0.5, sample_y, scaled):
                        mask[py][px] = 1

    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            filled = 0
            for sy in range(y * 4, (y + 1) * 4):
                for sx in range(x * 4, (x + 1) * 4):
                    filled += mask[sy][sx]
            row.append("1" if filled >= 4 else "0")
        rows.append("".join(row))

    return size, size, rows


def emit_bitmap(name, image):
    width, height, rows = image
    emitted_rows = ['    "' + row + '"' for row in rows]

    return "\n".join([
        f"static const int {name}_width = {width};",
        f"static const int {name}_height = {height};",
        f"static const char *const {name}_rows[{height}] = {{",
        ",\n".join(emitted_rows),
        "};",
    ])


def main():
    out_path = Path(sys.argv[1])
    guard = re.sub(r"[^A-Z0-9]+", "_", out_path.stem.upper()) + "_H"
    if len(sys.argv) == 5:
        icons = [
            ("continue_replay_icon", Path(sys.argv[2]), 48),
            ("finish_logout_icon", Path(sys.argv[3]), 48),
            ("best_crown_icon", Path(sys.argv[4]), 24),
        ]
    else:
        icons = []
        for spec in sys.argv[2:]:
            name, size, path = spec.split(":", 2)
            icons.append((name, Path(path), int(size)))

    rendered = [(name, render_svg(path, size)) for name, path, size in icons]
    parts = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    for name, image in rendered:
        parts.append(emit_bitmap(name, image))
        parts.append("")
    parts.append("#endif")
    parts.append("")

    out_path.write_text("\n".join(parts), encoding="ascii")


if __name__ == "__main__":
    main()
