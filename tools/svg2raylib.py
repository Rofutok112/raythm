#!/usr/bin/env python3
"""Convert simple SVG icons into raylib C++ drawing functions.

The converter intentionally targets single-color icon SVGs. It supports the
common geometry used by Lucide-style icons and similar UI assets:

- path commands: M, L, H, V, C, Q, Z and relative variants
- line, rect, circle, polyline, polygon

Unsupported SVG features such as transforms, gradients, masks, text, and
filters are reported as warnings and skipped or ignored.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import unquote, urlparse
from urllib.request import Request, urlopen


NUMBER_RE = re.compile(r"[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?")
PATH_TOKEN_RE = re.compile(r"[AaCcHhLlMmQqSsTtVvZz]|[-+]?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?")
SUPPORTED_PATH_COMMANDS = set("MmLlHhVvCcQqAaZz")
UNSUPPORTED_ELEMENTS = {
    "clipPath",
    "defs",
    "filter",
    "linearGradient",
    "mask",
    "pattern",
    "radialGradient",
    "style",
    "text",
}
MAX_SVG_BYTES = 2 * 1024 * 1024


@dataclass
class SvgSource:
    location: str
    name_hint: str
    content: str
    function_hint: str | None = None


@dataclass
class Segment:
    points: list[tuple[float, float]]
    closed: bool = False


@dataclass
class Icon:
    name: str
    view_box: tuple[float, float, float, float]
    segments: list[Segment] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


def local_name(tag: str) -> str:
    if "}" in tag:
        return tag.rsplit("}", 1)[1]
    return tag


def parse_float(value: str | None, fallback: float = 0.0) -> float:
    if not value:
        return fallback
    match = NUMBER_RE.search(value)
    return float(match.group(0)) if match else fallback


def parse_points(value: str | None) -> list[tuple[float, float]]:
    if not value:
        return []
    nums = [float(item.group(0)) for item in NUMBER_RE.finditer(value)]
    return list(zip(nums[0::2], nums[1::2]))


def function_name(name_hint: str) -> str:
    raw = re.sub(r"[^0-9A-Za-z_]+", "_", Path(name_hint).stem).strip("_").lower()
    if not raw:
        raw = "icon"
    if raw[0].isdigit():
        raw = f"icon_{raw}"
    return f"draw_{raw}"


def function_name_for_source(source: SvgSource) -> str:
    if source.function_hint:
        return function_name(source.function_hint)
    return function_name(source.name_hint)


def is_url(value: str) -> bool:
    return value.startswith("http://") or value.startswith("https://")


def source_name_from_url(value: str) -> str:
    parsed = urlparse(value)
    name = Path(unquote(parsed.path)).name
    return name or "icon.svg"


def load_svg_source(value: str) -> SvgSource:
    if is_url(value):
        request = Request(value, headers={"User-Agent": "raythm-svg2raylib/0.1"})
        with urlopen(request, timeout=20) as response:
            data = response.read(MAX_SVG_BYTES + 1)
        if len(data) > MAX_SVG_BYTES:
            raise ValueError(f"{value}: SVG is larger than {MAX_SVG_BYTES} bytes")
        return SvgSource(value, source_name_from_url(value), data.decode("utf-8"))

    path = Path(value)
    return SvgSource(str(path), path.name, path.read_text(encoding="utf-8"))


def load_manifest_sources(manifest_path: Path) -> list[SvgSource]:
    raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = raw.get("icons", raw) if isinstance(raw, dict) else raw
    if not isinstance(entries, list):
        raise ValueError(f"{manifest_path}: manifest must be a list or an object with an icons list")

    sources: list[SvgSource] = []
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise ValueError(f"{manifest_path}: icon entry {index} must be an object")
        source_value = entry.get("source") or entry.get("url") or entry.get("path")
        if not isinstance(source_value, str) or not source_value:
            raise ValueError(f"{manifest_path}: icon entry {index} is missing source")

        resolved_source = source_value
        if not is_url(source_value):
            resolved_source = str((manifest_path.parent / source_value).resolve())
        source = load_svg_source(resolved_source)
        name = entry.get("name")
        if name is not None:
            if not isinstance(name, str) or not name:
                raise ValueError(f"{manifest_path}: icon entry {index} name must be a non-empty string")
            source.function_hint = name
        sources.append(source)
    return sources


def parse_view_box(root: ET.Element) -> tuple[float, float, float, float]:
    view_box = root.attrib.get("viewBox")
    if view_box:
        values = [float(match.group(0)) for match in NUMBER_RE.finditer(view_box)]
        if len(values) == 4 and values[2] > 0 and values[3] > 0:
            return (values[0], values[1], values[2], values[3])
    width = parse_float(root.attrib.get("width"), 24.0)
    height = parse_float(root.attrib.get("height"), width)
    return (0.0, 0.0, max(width, 1.0), max(height, 1.0))


def cubic_point(p0, p1, p2, p3, t: float) -> tuple[float, float]:
    mt = 1.0 - t
    return (
        mt**3 * p0[0] + 3.0 * mt**2 * t * p1[0] + 3.0 * mt * t**2 * p2[0] + t**3 * p3[0],
        mt**3 * p0[1] + 3.0 * mt**2 * t * p1[1] + 3.0 * mt * t**2 * p2[1] + t**3 * p3[1],
    )


def quadratic_point(p0, p1, p2, t: float) -> tuple[float, float]:
    mt = 1.0 - t
    return (
        mt**2 * p0[0] + 2.0 * mt * t * p1[0] + t**2 * p2[0],
        mt**2 * p0[1] + 2.0 * mt * t * p1[1] + t**2 * p2[1],
    )


def vector_angle(u: tuple[float, float], v: tuple[float, float]) -> float:
    dot = u[0] * v[0] + u[1] * v[1]
    det = u[0] * v[1] - u[1] * v[0]
    return math.atan2(det, dot)


def arc_points(start: tuple[float, float],
               rx: float,
               ry: float,
               x_axis_rotation_degrees: float,
               large_arc: bool,
               sweep: bool,
               end: tuple[float, float]) -> list[tuple[float, float]]:
    rx = abs(rx)
    ry = abs(ry)
    if rx == 0.0 or ry == 0.0 or start == end:
        return [end]

    phi = math.radians(x_axis_rotation_degrees % 360.0)
    cos_phi = math.cos(phi)
    sin_phi = math.sin(phi)
    dx = (start[0] - end[0]) * 0.5
    dy = (start[1] - end[1]) * 0.5
    x1p = cos_phi * dx + sin_phi * dy
    y1p = -sin_phi * dx + cos_phi * dy

    radii_scale = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry)
    if radii_scale > 1.0:
        scale = math.sqrt(radii_scale)
        rx *= scale
        ry *= scale

    rx2 = rx * rx
    ry2 = ry * ry
    x1p2 = x1p * x1p
    y1p2 = y1p * y1p
    denominator = rx2 * y1p2 + ry2 * x1p2
    if denominator == 0.0:
        return [end]

    sign = -1.0 if large_arc == sweep else 1.0
    factor_sq = max(0.0, (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / denominator)
    factor = sign * math.sqrt(factor_sq)
    cxp = factor * (rx * y1p / ry)
    cyp = factor * (-ry * x1p / rx)

    cx = cos_phi * cxp - sin_phi * cyp + (start[0] + end[0]) * 0.5
    cy = sin_phi * cxp + cos_phi * cyp + (start[1] + end[1]) * 0.5

    theta1 = vector_angle((1.0, 0.0), ((x1p - cxp) / rx, (y1p - cyp) / ry))
    delta = vector_angle(((x1p - cxp) / rx, (y1p - cyp) / ry),
                         ((-x1p - cxp) / rx, (-y1p - cyp) / ry))
    if not sweep and delta > 0.0:
        delta -= math.tau
    elif sweep and delta < 0.0:
        delta += math.tau

    steps = max(2, int(math.ceil(abs(delta) / (math.pi / 8.0))))
    points: list[tuple[float, float]] = []
    for step in range(1, steps + 1):
        theta = theta1 + delta * (step / steps)
        x = cx + rx * math.cos(theta) * cos_phi - ry * math.sin(theta) * sin_phi
        y = cy + rx * math.cos(theta) * sin_phi + ry * math.sin(theta) * cos_phi
        points.append((x, y))
    return points


def path_tokens(d: str) -> list[str]:
    return [match.group(0) for match in PATH_TOKEN_RE.finditer(d)]


def is_command(token: str) -> bool:
    return len(token) == 1 and token.isalpha()


def parse_path(d: str, warnings: list[str], curve_steps: int) -> list[Segment]:
    tokens = path_tokens(d)
    segments: list[Segment] = []
    index = 0
    command = ""
    current = (0.0, 0.0)
    start = (0.0, 0.0)
    active: list[tuple[float, float]] = []

    def finish(closed: bool = False) -> None:
        nonlocal active
        if len(active) >= 2:
            segments.append(Segment(active, closed))
        active = []

    def read_number() -> float:
        nonlocal index
        if index >= len(tokens) or is_command(tokens[index]):
            raise ValueError("path command is missing a numeric parameter")
        value = float(tokens[index])
        index += 1
        return value

    def has_number() -> bool:
        return index < len(tokens) and not is_command(tokens[index])

    while index < len(tokens):
        if is_command(tokens[index]):
            command = tokens[index]
            index += 1
            if command not in SUPPORTED_PATH_COMMANDS:
                warnings.append(f"Unsupported path command {command}; remaining command parameters were skipped.")
                while index < len(tokens) and not is_command(tokens[index]):
                    index += 1
                continue
        if not command:
            raise ValueError("path data must start with a command")

        relative = command.islower()
        upper = command.upper()

        if upper == "M":
            x = read_number()
            y = read_number()
            current = (current[0] + x, current[1] + y) if relative else (x, y)
            finish()
            active = [current]
            start = current
            command = "l" if relative else "L"
            continue

        if upper == "Z":
            if active and active[-1] != start:
                active.append(start)
            current = start
            finish(True)
            continue

        if upper == "L":
            while has_number():
                x = read_number()
                y = read_number()
                current = (current[0] + x, current[1] + y) if relative else (x, y)
                active.append(current)
            continue

        if upper == "H":
            while has_number():
                x = read_number()
                current = (current[0] + x, current[1]) if relative else (x, current[1])
                active.append(current)
            continue

        if upper == "V":
            while has_number():
                y = read_number()
                current = (current[0], current[1] + y) if relative else (current[0], y)
                active.append(current)
            continue

        if upper == "C":
            while has_number():
                c1 = (read_number(), read_number())
                c2 = (read_number(), read_number())
                end = (read_number(), read_number())
                if relative:
                    c1 = (current[0] + c1[0], current[1] + c1[1])
                    c2 = (current[0] + c2[0], current[1] + c2[1])
                    end = (current[0] + end[0], current[1] + end[1])
                for step in range(1, curve_steps + 1):
                    active.append(cubic_point(current, c1, c2, end, step / curve_steps))
                current = end
            continue

        if upper == "Q":
            while has_number():
                control = (read_number(), read_number())
                end = (read_number(), read_number())
                if relative:
                    control = (current[0] + control[0], current[1] + control[1])
                    end = (current[0] + end[0], current[1] + end[1])
                for step in range(1, curve_steps + 1):
                    active.append(quadratic_point(current, control, end, step / curve_steps))
                current = end
            continue

        if upper == "A":
            while has_number():
                rx = read_number()
                ry = read_number()
                x_axis_rotation = read_number()
                large_arc = read_number() != 0.0
                sweep = read_number() != 0.0
                end = (read_number(), read_number())
                if relative:
                    end = (current[0] + end[0], current[1] + end[1])
                active.extend(arc_points(current, rx, ry, x_axis_rotation, large_arc, sweep, end))
                current = end
            continue

    finish()
    return segments


def circle_segment(cx: float, cy: float, radius: float, steps: int) -> Segment:
    points = [
        (cx + math.cos((math.tau * index) / steps) * radius, cy + math.sin((math.tau * index) / steps) * radius)
        for index in range(steps)
    ]
    points.append(points[0])
    return Segment(points, True)


def rect_segment(x: float, y: float, width: float, height: float) -> Segment:
    return Segment([(x, y), (x + width, y), (x + width, y + height), (x, y + height), (x, y)], True)


def should_skip_element(element: ET.Element, warnings: list[str]) -> bool:
    tag = local_name(element.tag)
    if tag in UNSUPPORTED_ELEMENTS:
        warnings.append(f"Unsupported <{tag}> element was skipped.")
        return True
    if "transform" in element.attrib:
        warnings.append(f"transform on <{tag}> is not supported and was ignored.")
    return False


def parse_svg(source: SvgSource, curve_steps: int, circle_steps: int) -> Icon:
    root = ET.fromstring(source.content)
    icon = Icon(function_name_for_source(source), parse_view_box(root))

    for element in root.iter():
        if element is root or should_skip_element(element, icon.warnings):
            continue
        tag = local_name(element.tag)
        try:
            if tag == "path":
                d = element.attrib.get("d", "")
                if d:
                    icon.segments.extend(parse_path(d, icon.warnings, curve_steps))
            elif tag == "line":
                x1 = parse_float(element.attrib.get("x1"))
                y1 = parse_float(element.attrib.get("y1"))
                x2 = parse_float(element.attrib.get("x2"))
                y2 = parse_float(element.attrib.get("y2"))
                icon.segments.append(Segment([(x1, y1), (x2, y2)]))
            elif tag == "polyline":
                points = parse_points(element.attrib.get("points"))
                if len(points) >= 2:
                    icon.segments.append(Segment(points))
            elif tag == "polygon":
                points = parse_points(element.attrib.get("points"))
                if len(points) >= 2:
                    icon.segments.append(Segment(points + [points[0]], True))
            elif tag == "rect":
                x = parse_float(element.attrib.get("x"))
                y = parse_float(element.attrib.get("y"))
                width = parse_float(element.attrib.get("width"))
                height = parse_float(element.attrib.get("height"))
                if width > 0 and height > 0:
                    if parse_float(element.attrib.get("rx")) > 0 or parse_float(element.attrib.get("ry")) > 0:
                        icon.warnings.append("Rounded rect rx/ry is not supported; emitted a square-corner rect.")
                    icon.segments.append(rect_segment(x, y, width, height))
            elif tag == "circle":
                radius = parse_float(element.attrib.get("r"))
                if radius > 0:
                    icon.segments.append(circle_segment(
                        parse_float(element.attrib.get("cx")),
                        parse_float(element.attrib.get("cy")),
                        radius,
                        circle_steps,
                    ))
            elif tag not in {"g", "svg", "title"}:
                icon.warnings.append(f"Unsupported <{tag}> element was skipped.")
        except ValueError as error:
            icon.warnings.append(f"{tag} parse error: {error}")

    return icon


def fmt(value: float) -> str:
    if abs(value) < 0.000001:
        value = 0.0
    text = f"{value:.6g}"
    if "e" not in text.lower() and "." not in text:
        text += ".0"
    return f"{text}f"


def emit_icon(icon: Icon) -> str:
    min_x, min_y, width, height = icon.view_box
    lines = [
        f"void {icon.name}(Rectangle bounds, Color color, float stroke_width) {{",
        f"    constexpr float svg_min_x = {fmt(min_x)};",
        f"    constexpr float svg_min_y = {fmt(min_y)};",
        f"    constexpr float svg_width = {fmt(width)};",
        f"    constexpr float svg_height = {fmt(height)};",
    ]
    for segment_index, segment in enumerate(icon.segments):
        lines.append(f"    Vector2 points_{segment_index}[] = {{")
        for x, y in segment.points:
            lines.append(f"        svg2raylib_point(bounds, svg_min_x, svg_min_y, svg_width, svg_height, {fmt(x)}, {fmt(y)}),")
        lines.append("    };")
        lines.append(f"    svg2raylib_polyline(points_{segment_index}, {len(segment.points)}, color, stroke_width);")
    if not icon.segments:
        lines.append("    (void)bounds;")
        lines.append("    (void)color;")
        lines.append("    (void)stroke_width;")
    lines.append("}")
    return "\n".join(lines)


def emit_cpp(icons: list[Icon], namespace: str, header: str | None) -> str:
    lines = [
        "// Generated by tools/svg2raylib.py. Do not edit by hand.",
        "#include \"raylib.h\"",
        "",
        "#include <cstddef>",
    ]
    if header:
        lines.insert(1, f"#include \"{header}\"")
    lines.extend([
        "",
        f"namespace {namespace} {{",
        "namespace {",
        "",
        "Vector2 svg2raylib_point(Rectangle bounds, float min_x, float min_y, float width, float height, float x, float y) {",
        "    return {",
        "        bounds.x + ((x - min_x) / width) * bounds.width,",
        "        bounds.y + ((y - min_y) / height) * bounds.height,",
        "    };",
        "}",
        "",
        "void svg2raylib_polyline(const Vector2* points, size_t count, Color color, float stroke_width) {",
        "    if (count < 2) {",
        "        return;",
        "    }",
        "    for (size_t index = 1; index < count; ++index) {",
        "        DrawLineEx(points[index - 1], points[index], stroke_width, color);",
        "    }",
        "}",
        "",
        "}  // namespace",
        "",
    ])
    for icon in icons:
        lines.append(emit_icon(icon))
        lines.append("")
    lines.append(f"}}  // namespace {namespace}")
    return "\n".join(lines)


def emit_header(icons: list[Icon], namespace: str) -> str:
    lines = [
        "// Generated by tools/svg2raylib.py. Do not edit by hand.",
        "#pragma once",
        "",
        "#include \"raylib.h\"",
        "",
        f"namespace {namespace} {{",
    ]
    for icon in icons:
        lines.append(f"void {icon.name}(Rectangle bounds, Color color, float stroke_width = 2.0f);")
    lines.append(f"}}  // namespace {namespace}")
    return "\n".join(lines)


def point_segment_distance(px: float, py: float, a: tuple[float, float], b: tuple[float, float]) -> float:
    vx = b[0] - a[0]
    vy = b[1] - a[1]
    wx = px - a[0]
    wy = py - a[1]
    length_sq = vx * vx + vy * vy
    if length_sq <= 0.000001:
        return math.hypot(px - a[0], py - a[1])
    t = max(0.0, min(1.0, (wx * vx + wy * vy) / length_sq))
    closest_x = a[0] + vx * t
    closest_y = a[1] + vy * t
    return math.hypot(px - closest_x, py - closest_y)


def icon_sdf_pixels(icon: Icon, cell_size: int) -> list[int]:
    min_x, min_y, width, height = icon.view_box
    pad = max(width, height) * 0.12
    sample_min_x = min_x - pad
    sample_min_y = min_y - pad
    sample_width = width + pad * 2.0
    sample_height = height + pad * 2.0
    stroke_radius = max(width, height) / 24.0
    spread = max(width, height) / 8.0
    pixels: list[int] = []

    for y in range(cell_size):
        svg_y = sample_min_y + ((y + 0.5) / cell_size) * sample_height
        for x in range(cell_size):
            svg_x = sample_min_x + ((x + 0.5) / cell_size) * sample_width
            distance = 1.0e9
            for segment in icon.segments:
                for index in range(1, len(segment.points)):
                    distance = min(distance, point_segment_distance(svg_x, svg_y,
                                                                    segment.points[index - 1],
                                                                    segment.points[index]))
            signed_distance = stroke_radius - distance
            alpha = int(max(0.0, min(255.0, 128.0 + signed_distance * 128.0 / spread)))
            pixels.extend([255, 255, 255, alpha])
    return pixels


def emit_byte_array(name: str, values: list[int], indent: str = "") -> list[str]:
    lines = [f"{indent}constexpr unsigned char {name}[] = {{"]
    for index in range(0, len(values), 16):
        chunk = ", ".join(str(value) for value in values[index:index + 16])
        lines.append(f"{indent}    {chunk},")
    lines.append(f"{indent}}};")
    return lines


def emit_sdf_cpp(icons: list[Icon], namespace: str, header: str | None, cell_size: int) -> str:
    atlas_width = cell_size
    atlas_height = max(1, cell_size * len(icons))
    pixels: list[int] = []
    for icon in icons:
        pixels.extend(icon_sdf_pixels(icon, cell_size))

    lines = [
        "// Generated by tools/svg2raylib.py. Do not edit by hand.",
    ]
    if header:
        lines.append(f"#include \"{header}\"")
    lines.extend([
        "#include \"raylib.h\"",
        "",
        f"namespace {namespace} {{",
        "namespace {",
        "",
        f"constexpr int kIconAtlasWidth = {atlas_width};",
        f"constexpr int kIconAtlasHeight = {atlas_height};",
        f"constexpr int kIconCellSize = {cell_size};",
        "",
        "constexpr const char* kIconSdfFragmentShader = R\"(",
        "#version 330",
        "",
        "in vec2 fragTexCoord;",
        "in vec4 fragColor;",
        "",
        "uniform sampler2D texture0;",
        "",
        "out vec4 finalColor;",
        "",
        "void main()",
        "{",
        "    float distanceFromOutline = texture(texture0, fragTexCoord).a - 0.5;",
        "    float distanceChangePerFragment = max(length(vec2(dFdx(distanceFromOutline), dFdy(distanceFromOutline))), 0.001);",
        "    float alpha = smoothstep(-distanceChangePerFragment, distanceChangePerFragment, distanceFromOutline);",
        "    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);",
        "}",
        ")\";",
        "",
    ])
    lines.extend(emit_byte_array("kIconAtlasPixels", pixels))
    lines.extend([
        "",
        "Texture2D& icon_atlas_texture() {",
        "    static Texture2D texture = {};",
        "    if (texture.id == 0) {",
        "        Image image = {",
        "            const_cast<unsigned char*>(kIconAtlasPixels),",
        "            kIconAtlasWidth,",
        "            kIconAtlasHeight,",
        "            1,",
        "            PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,",
        "        };",
        "        texture = LoadTextureFromImage(image);",
        "        if (texture.id != 0) {",
        "            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);",
        "        }",
        "    }",
        "    return texture;",
        "}",
        "",
        "Shader& icon_sdf_shader() {",
        "    static Shader shader = {};",
        "    if (shader.id == 0) {",
        "        shader = LoadShaderFromMemory(nullptr, kIconSdfFragmentShader);",
        "    }",
        "    return shader;",
        "}",
        "",
        "void draw_sdf_icon(Rectangle source, Rectangle bounds, Color color) {",
        "    Texture2D& texture = icon_atlas_texture();",
        "    if (texture.id == 0) {",
        "        return;",
        "    }",
        "    Shader& shader = icon_sdf_shader();",
        "    if (shader.id != 0) {",
        "        BeginShaderMode(shader);",
        "    }",
        "    DrawTexturePro(texture, source, bounds, {0.0f, 0.0f}, 0.0f, color);",
        "    if (shader.id != 0) {",
        "        EndShaderMode();",
        "    }",
        "}",
        "",
        "}  // namespace",
        "",
    ])
    for index, icon in enumerate(icons):
        lines.extend([
            f"void {icon.name}(Rectangle bounds, Color color, float stroke_width) {{",
            "    (void)stroke_width;",
            f"    draw_sdf_icon({{0.0f, {fmt(index * cell_size)}, static_cast<float>(kIconCellSize), static_cast<float>(kIconCellSize)}},",
            "                  bounds, color);",
            "}",
            "",
        ])
    lines.append(f"}}  // namespace {namespace}")
    return "\n".join(lines)


def emit_preview_main(icons: list[Icon], namespace: str, header: str) -> str:
    lines = [
        "// Generated by tools/svg2raylib.py. Use this as a quick raylib preview harness.",
        f"#include \"{header}\"",
        "",
        "#include \"raylib.h\"",
        "",
        "int main() {",
        "    InitWindow(960, 540, \"svg2raylib preview\");",
        "    SetTargetFPS(60);",
        "",
        "    while (!WindowShouldClose()) {",
        "        BeginDrawing();",
        "        ClearBackground({24, 24, 28, 255});",
        "",
        "        const Color icon_color = {240, 240, 245, 255};",
        "        const Color text_color = {180, 180, 190, 255};",
        "",
    ]
    for index, icon in enumerate(icons):
        column = index % 6
        row = index // 6
        lines.extend([
            "        {",
            f"            Rectangle cell = {{{fmt(32 + column * 150)}, {fmt(32 + row * 150)}, 112.0f, 112.0f}};",
            "            DrawRectangleLinesEx(cell, 1.0f, {70, 70, 78, 255});",
            f"            {namespace}::{icon.name}({{cell.x + 24.0f, cell.y + 16.0f, 64.0f, 64.0f}}, icon_color, 2.0f);",
            f"            DrawText(\"{icon.name}\", static_cast<int>(cell.x), static_cast<int>(cell.y + 88.0f), 12, text_color);",
            "        }",
            "",
        ])
    lines.extend([
        "        EndDrawing();",
        "    }",
        "",
        "    CloseWindow();",
        "    return 0;",
        "}",
    ])
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Convert simple SVG icons to raylib C++ drawing code.")
    parser.add_argument("svg", nargs="*", help="Input SVG file(s) or URL(s)")
    parser.add_argument("--manifest", type=Path, help="Optional JSON manifest of icons to generate")
    parser.add_argument("--out", type=Path, required=True, help="Output .cpp path")
    parser.add_argument("--header-out", type=Path, help="Optional output .h path")
    parser.add_argument("--include", help="Header include path to emit in the .cpp")
    parser.add_argument("--preview-main", type=Path, help="Optional raylib preview main .cpp path")
    parser.add_argument("--namespace", default="raythm_icons", help="C++ namespace")
    parser.add_argument("--sdf-atlas", action="store_true", help="Emit embedded SDF atlas drawing code instead of line primitives")
    parser.add_argument("--sdf-cell-size", type=int, default=64, help="SDF atlas cell size in pixels")
    parser.add_argument("--curve-steps", type=int, default=12, help="Segments per cubic/quadratic curve")
    parser.add_argument("--circle-steps", type=int, default=32, help="Segments per circle")
    args = parser.parse_args(argv)

    if args.preview_main and not args.header_out and not args.include:
        parser.error("--preview-main requires --header-out or --include")

    sources: list[SvgSource] = []
    if args.manifest:
        sources.extend(load_manifest_sources(args.manifest))
    sources.extend(load_svg_source(value) for value in args.svg)
    if not sources:
        parser.error("at least one SVG input or --manifest is required")

    icons = [parse_svg(source, max(2, args.curve_steps), max(8, args.circle_steps)) for source in sources]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    if args.sdf_atlas:
        args.out.write_text(emit_sdf_cpp(icons, args.namespace, args.include, max(16, args.sdf_cell_size)),
                            encoding="utf-8", newline="\n")
    else:
        args.out.write_text(emit_cpp(icons, args.namespace, args.include), encoding="utf-8", newline="\n")
    if args.header_out:
        args.header_out.parent.mkdir(parents=True, exist_ok=True)
        args.header_out.write_text(emit_header(icons, args.namespace), encoding="utf-8", newline="\n")
    if args.preview_main:
        preview_header = args.include or args.header_out.name
        args.preview_main.parent.mkdir(parents=True, exist_ok=True)
        args.preview_main.write_text(emit_preview_main(icons, args.namespace, preview_header), encoding="utf-8", newline="\n")

    for source, icon in zip(sources, icons):
        for warning in icon.warnings:
            print(f"{source.location}: {icon.name}: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
