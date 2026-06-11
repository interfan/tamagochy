from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "kawaii-companion-concepts.png"
NEW_COMPANIONS_SOURCE = ROOT / "assets" / "kawaii-new-companions.png"
EGG_SOURCE = ROOT / "assets" / "kawaii-egg-concept.png"
OUTPUT = ROOT / "companion_bitmaps.h"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
ACTION_SCENE_DIR = ROOT / "assets" / "action-scenes"
ACTION_SCENES = (
    "feed", "water", "sleep", "overnight", "clean",
    "medicine", "learn", "pet", "groom", "wash",
)
SPECS = {
    "cat": {
        "crop": (0, 100, 570, 770), "width": 88, "height": 96, "threshold": 170,
        "eyes": [(39, 38), (55, 38)], "mouth": (47, 51),
    },
    "dog": {
        "crop": (585, 155, 1145, 770), "width": 96, "height": 100, "threshold": 158,
        "eyes": [(39, 46), (57, 46)], "mouth": (48, 59),
    },
    "bunny": {
        "crop": (1190, 130, 1745, 770), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(42, 43), (58, 43)], "mouth": (50, 55),
    },
}
NEW_COMPANION_SPECS = {
    "panda": {
        "crop": (20, 135, 390, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(40, 42), (57, 42)], "mouth": (49, 56),
    },
    "dragon": {
        "crop": (390, 120, 810, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(40, 42), (57, 42)], "mouth": (49, 55),
    },
    "fox": {
        "crop": (805, 115, 1210, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(40, 43), (57, 43)], "mouth": (49, 56),
    },
    "chicken": {
        "crop": (1210, 135, 1570, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(40, 43), (57, 43)], "mouth": (49, 57),
    },
    "pig": {
        "crop": (1570, 145, 1950, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(39, 43), (58, 43)], "mouth": (49, 61),
    },
}


def prepare_bitmap(image: Image.Image, spec: dict) -> Image.Image:
    crop = spec["crop"]
    width = spec["width"]
    height = spec["height"]
    animal = image.crop(crop).convert("L")
    animal = ImageOps.autocontrast(animal)
    animal = ImageEnhance.Contrast(animal).enhance(2.0)
    animal.thumbnail((width - 2, height - 2), Image.Resampling.LANCZOS)

    canvas = Image.new("L", (width, height), 255)
    x = (width - animal.width) // 2
    y = height - animal.height - 1
    canvas.paste(animal, (x, y))
    return canvas.point(lambda value: 0 if value < spec["threshold"] else 255)


def encode_bitmap(image: Image.Image, width: int, height: int) -> list[int]:
    values = []
    pixels = image.load()
    for y in range(height):
        for byte_x in range((width + 7) // 8):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width and pixels[x, y] == 0:
                    value |= 0x80 >> bit
            values.append(value)
    return values


def connected_components(image: Image.Image) -> list[tuple[int, int, int, int, int, int]]:
    binary = image.point(lambda value: 1 if value < 235 else 0)
    pixels = binary.load()
    width, height = binary.size
    seen = bytearray(width * height)
    components = []
    for y in range(height):
        for x in range(width):
            index = y * width + x
            if not pixels[x, y] or seen[index]:
                continue
            stack = [(x, y)]
            seen[index] = 1
            min_x = max_x = sum_x = x
            min_y = max_y = sum_y = y
            count = 1
            while stack:
                px, py = stack.pop()
                for nx, ny in ((px - 1, py), (px + 1, py), (px, py - 1), (px, py + 1)):
                    if not (0 <= nx < width and 0 <= ny < height):
                        continue
                    neighbor = ny * width + nx
                    if pixels[nx, ny] and not seen[neighbor]:
                        seen[neighbor] = 1
                        stack.append((nx, ny))
                        min_x = min(min_x, nx)
                        max_x = max(max_x, nx)
                        min_y = min(min_y, ny)
                        max_y = max(max_y, ny)
                        sum_x += nx
                        sum_y += ny
                        count += 1
            components.append((min_x, min_y, max_x + 1, max_y + 1, sum_x // count, count))
    return components


def prepare_action_frames(
    image: Image.Image,
    source_frames: list[tuple[tuple[int, int, int, int], list[tuple[int, int, int, int]]]] | None = None,
) -> list[Image.Image]:
    rgba = image.convert("RGBA")
    white = Image.new("RGBA", rgba.size, "white")
    white.alpha_composite(rgba)
    source = white.convert("L")
    cropped = []
    if source_frames:
        for crop, masks in source_frames:
            part = source.crop(crop)
            draw = ImageDraw.Draw(part)
            for mask in masks:
                draw.rectangle(mask, fill=255)
            ink = part.point(lambda value: 0 if value < 235 else 255)
            bbox = ImageOps.invert(ink).getbbox()
            cropped.append(part.crop(bbox) if bbox else part)
    else:
        frame_width = source.width // 4
        components = connected_components(source)
        for frame in range(4):
            start_x = frame * frame_width
            end_x = (frame + 1) * frame_width
            part = Image.new("L", source.size, 255)
            for min_x, min_y, max_x, max_y, center_x, count in components:
                if start_x <= center_x < end_x and count >= 4:
                    part.paste(source.crop((min_x, min_y, max_x, max_y)), (min_x, min_y))
            ink = part.point(lambda value: 0 if value < 235 else 255)
            bbox = ImageOps.invert(ink).getbbox()
            cropped.append(part.crop(bbox) if bbox else part)

    max_width = max(frame.width for frame in cropped)
    max_height = max(frame.height for frame in cropped)
    scale = min(178 / max_width, 102 / max_height)
    prepared = []
    for frame in cropped:
        resized = frame.resize(
            (max(1, round(frame.width * scale)), max(1, round(frame.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = ImageOps.autocontrast(resized)
        resized = ImageEnhance.Contrast(resized).enhance(2.0)
        canvas = Image.new("L", (184, 108), 255)
        x = (184 - resized.width) // 2
        y = 108 - resized.height - 2
        canvas.paste(resized, (x, y))
        prepared.append(canvas.point(lambda value: 0 if value < 180 else 255))
    return prepared


def clean_action_frame(action: str, frame_number: int, frame: Image.Image) -> Image.Image:
    cleaned = frame.copy()
    draw = ImageDraw.Draw(cleaned)
    masks = {
        ("feed", 0): [(136, 86, 183, 107)],
        ("feed", 2): [(132, 84, 183, 107)],
        ("groom", 2): [(35, 76, 47, 87)],
    }
    for rectangle in masks.get((action, frame_number), []):
        draw.rectangle(rectangle, fill=255)
    return cleaned


def blink_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    pose = bitmap.copy()
    draw = ImageDraw.Draw(pose)
    for x, y in spec["eyes"]:
        draw.rectangle((x - 4, y - 4, x + 4, y + 4), fill=255)
        draw.arc((x - 4, y - 2, x + 4, y + 4), 195, 345, fill=0, width=1)
    return pose


def eat_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    # A slight forward lean makes the head visibly dip toward a bowl on the left.
    tilted = bitmap.rotate(8, resample=Image.Resampling.NEAREST, expand=False, fillcolor=255)
    pose = Image.new("L", bitmap.size, 255)
    pose.paste(tilted, (-3, 4))
    draw = ImageDraw.Draw(pose)
    mx, my = spec["mouth"]
    mx -= 3
    my += 4
    draw.rectangle((mx - 5, my - 4, mx + 5, my + 5), fill=255)
    draw.ellipse((mx - 4, my - 2, mx + 4, my + 5), outline=0, width=2)
    draw.line((mx - 2, my + 1, mx + 2, my + 1), fill=0)
    return pose


def happy_pose(bitmap: Image.Image, spec: dict, animal: str) -> Image.Image:
    pose = blink_pose(bitmap, spec)
    draw = ImageDraw.Draw(pose)
    mx, my = spec["mouth"]
    draw.rectangle((mx - 5, my - 4, mx + 5, my + 5), fill=255)
    draw.arc((mx - 5, my - 5, mx + 5, my + 5), 15, 165, fill=0, width=2)
    if animal == "cat":
        draw.rectangle((0, 51, 30, 80), fill=255)
        draw.arc((2, 45, 27, 70), 80, 285, fill=0, width=2)
        draw.arc((7, 50, 22, 65), 80, 285, fill=0, width=1)
        draw.line((22, 64, 31, 72), fill=0, width=2)
    elif animal == "dog":
        draw.rectangle((77, 48, 95, 76), fill=255)
        draw.line((76, 63, 91, 48), fill=0, width=2)
        draw.line((91, 48, 86, 61), fill=0, width=2)
        draw.line((86, 61, 95, 62), fill=0, width=2)
        draw.line((95, 62, 79, 72), fill=0, width=2)
    elif animal == "bunny":
        draw.ellipse((76, 60, 94, 78), outline=0, width=2)
    return pose


def sleep_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    pose = blink_pose(bitmap, spec)
    draw = ImageDraw.Draw(pose)
    mx, my = spec["mouth"]
    draw.rectangle((mx - 5, my - 4, mx + 5, my + 5), fill=255)
    draw.ellipse((mx - 3, my - 2, mx + 3, my + 4), outline=0, width=2)
    return pose


def format_array(name: str, values: list[int]) -> str:
    lines = []
    for start in range(0, len(values), 12):
        chunk = ", ".join(f"0x{value:02X}" for value in values[start:start + 12])
        lines.append(f"  {chunk},")
    return f"const uint8_t {name}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};\n"


def encode_rle(image: Image.Image) -> list[int]:
    pixels = image.load()
    runs = []
    black = False
    count = 0
    for y in range(image.height):
        for x in range(image.width):
            pixel_black = pixels[x, y] == 0
            if pixel_black == black:
                if count == 255:
                    # A zero-length opposite-color run preserves the current color.
                    runs.extend((255, 0))
                    count = 0
                count += 1
            else:
                runs.append(count)
                black = not black
                count = 1
    runs.append(count)
    return runs


def validate_rle(image: Image.Image, runs: list[int]) -> None:
    expected = [image.getpixel((x, y)) == 0 for y in range(image.height) for x in range(image.width)]
    decoded = []
    black = False
    for count in runs:
        decoded.extend([black] * count)
        black = not black
    if decoded != expected:
        raise ValueError("RLE round-trip validation failed")


def main() -> None:
    image = Image.open(SOURCE)
    new_companions = Image.open(NEW_COMPANIONS_SOURCE)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    output = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
    ]

    for name, spec in SPECS.items():
        bitmap = prepare_bitmap(image, spec)
        bitmap.save(PREVIEW_DIR / f"{name}.png")
        prefix = name.upper()
        output.append(f"const uint8_t {prefix}_WIDTH = {spec['width']};")
        output.append(f"const uint8_t {prefix}_HEIGHT = {spec['height']};")
        output.append(format_array(
            f"{prefix}_BITMAP",
            encode_bitmap(bitmap, spec["width"], spec["height"]),
        ))
        for pose_name, pose in (
            ("BLINK", blink_pose(bitmap, spec)),
            ("EAT", eat_pose(bitmap, spec)),
            ("HAPPY", happy_pose(bitmap, spec, name)),
            ("SLEEP", sleep_pose(bitmap, spec)),
        ):
            pose.save(PREVIEW_DIR / f"{name}-{pose_name.lower()}.png")
            output.append(format_array(
                f"{prefix}_{pose_name}_BITMAP",
                encode_bitmap(pose, spec["width"], spec["height"]),
            ))

    for name, spec in NEW_COMPANION_SPECS.items():
        bitmap = prepare_bitmap(new_companions, spec)
        bitmap.save(PREVIEW_DIR / f"{name}.png")
        prefix = name.upper()
        output.append(f"const uint8_t {prefix}_WIDTH = {spec['width']};")
        output.append(f"const uint8_t {prefix}_HEIGHT = {spec['height']};")
        output.append(format_array(
            f"{prefix}_BITMAP",
            encode_bitmap(bitmap, spec["width"], spec["height"]),
        ))

    output.append("const uint8_t CAT_ACTION_SCENE_WIDTH = 184;")
    output.append("const uint8_t CAT_ACTION_SCENE_HEIGHT = 108;")
    for action in ACTION_SCENES:
        feed_source_frames = [
            ((25, 130, 510, 625), []),
            ((490, 190, 975, 625), [(0, 0, 35, 330)]),
            ((985, 220, 1435, 625), []),
            ((1400, 140, 1915, 625), [(0, 0, 55, 365)]),
        ]
        source_frames = feed_source_frames if action == "feed" else None
        frames = prepare_action_frames(
            Image.open(ACTION_SCENE_DIR / f"{action}.png"),
            source_frames=source_frames,
        )
        contact_sheet = Image.new("L", (184 * 4, 108), 255)
        for frame_number, frame in enumerate(frames):
            frame = clean_action_frame(action, frame_number, frame)
            frame.save(PREVIEW_DIR / f"cat-{action}-{frame_number}.png")
            contact_sheet.paste(frame, (184 * frame_number, 0))
            if action != "overnight":
                runs = encode_rle(frame)
                validate_rle(frame, runs)
                output.append(format_array(
                    f"CAT_{action.upper()}_{frame_number}_RLE",
                    runs,
                ))
        contact_sheet.save(PREVIEW_DIR / f"cat-{action}-sheet.png")

    egg_source = Image.open(EGG_SOURCE)
    egg_spec = {"crop": (80, 55, 1175, 1190), "width": 92, "height": 100, "threshold": 165}
    egg = prepare_bitmap(egg_source, egg_spec)
    egg.save(PREVIEW_DIR / "egg.png")
    output.append("const uint8_t EGG_WIDTH = 92;")
    output.append("const uint8_t EGG_HEIGHT = 100;")
    output.append(format_array("EGG_BITMAP", encode_bitmap(egg, 92, 100)))

    OUTPUT.write_text("\n".join(output), encoding="ascii")


if __name__ == "__main__":
    main()
