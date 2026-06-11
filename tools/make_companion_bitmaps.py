from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "kawaii-companion-concepts.png"
NEW_COMPANIONS_SOURCE = ROOT / "assets" / "kawaii-new-companions.png"
FINAL_COMPANIONS_SOURCE = ROOT / "assets" / "kawaii-final-companions.png"
EGG_SOURCE = ROOT / "assets" / "kawaii-egg-concept.png"
OUTPUT = ROOT / "companion_bitmaps.h"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
ACTION_SCENE_DIR = ROOT / "assets" / "action-scenes"
SPECIES_FEED_COMPANIONS_SOURCE = ACTION_SCENE_DIR / "species-feed-companions.png"
SPECIES_FEED_SPECIAL_SOURCE = ACTION_SCENE_DIR / "species-feed-dragon-fox-penguin.png"
SPECIES_WATER_COMPANIONS_SOURCE = ACTION_SCENE_DIR / "species-water-companions.png"
SPECIES_WATER_SPECIAL_SOURCE = ACTION_SCENE_DIR / "species-water-dragon-fox-penguin.png"
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
        "eyes": [(39, 44), (57, 44)], "eye_radius": 6, "mouth": (49, 56),
    },
    "dragon": {
        "crop": (390, 120, 810, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(41, 46), (56, 46)], "eye_radius": 6, "mouth": (49, 55),
    },
    "fox": {
        "crop": (805, 115, 1210, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(41, 48), (56, 48)], "eye_radius": 6, "mouth": (49, 57),
    },
    "chicken": {
        "crop": (1210, 135, 1570, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(42, 46), (56, 46)], "eye_radius": 6, "mouth": (49, 57),
    },
    "pig": {
        "crop": (1570, 145, 1950, 650), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(41, 48), (57, 48)], "eye_radius": 6, "mouth": (49, 58),
    },
}
FINAL_COMPANION_SPECS = {
    "hamster": {
        "crop": (230, 75, 845, 805), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(39, 45), (58, 45)], "eye_radius": 6, "mouth": (49, 57),
    },
    "penguin": {
        "crop": (930, 75, 1540, 805), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(39, 43), (58, 43)], "eye_radius": 6, "mouth": (49, 55),
    },
}
SPECIES_FEED_SHEETS = (
    (SPECIES_FEED_COMPANIONS_SOURCE, ("dog", "bunny", "panda", "chicken", "pig", "hamster")),
    (SPECIES_FEED_SPECIAL_SOURCE, ("dragon", "fox", "penguin")),
)
SPECIES_WATER_SHEETS = (
    (SPECIES_WATER_COMPANIONS_SOURCE, ("dog", "bunny", "panda", "chicken", "pig", "hamster")),
    (SPECIES_WATER_SPECIAL_SOURCE, ("dragon", "fox", "penguin")),
)


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


def encode_spans(image: Image.Image) -> list[int]:
    values = []
    pixels = image.load()
    for y in range(image.height):
        x = 0
        while x < image.width:
            while x < image.width and pixels[x, y] != 0:
                x += 1
            if x >= image.width:
                break
            start = x
            while x < image.width and pixels[x, y] == 0:
                x += 1
            values.extend((start, x - start))
        values.append(255)
    return values


def validate_spans(image: Image.Image, spans: list[int]) -> None:
    decoded = Image.new("L", image.size, 255)
    draw = ImageDraw.Draw(decoded)
    index = 0
    for y in range(image.height):
        while True:
            start = spans[index]
            index += 1
            if start == 255:
                break
            length = spans[index]
            index += 1
            draw.line((start, y, start + length - 1, y), fill=0)
    if decoded.tobytes() != image.tobytes():
        raise ValueError("Span round-trip validation failed")


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


def prepare_grid_action_frames(image: Image.Image, row: int, row_count: int) -> list[Image.Image]:
    source = ImageOps.autocontrast(image.convert("L"))
    components = connected_components(source)
    cell_width = source.width / 4
    cell_height = source.height / row_count
    prepared = []
    for column in range(4):
        part = Image.new("L", source.size, 255)
        for min_x, min_y, max_x, max_y, center_x, count in components:
            center_y = (min_y + max_y) // 2
            if (column * cell_width <= center_x < (column + 1) * cell_width and
                    row * cell_height <= center_y < (row + 1) * cell_height and count >= 4):
                part.paste(source.crop((min_x, min_y, max_x, max_y)), (min_x, min_y))
        ink = part.point(lambda value: 0 if value < 225 else 255)
        bbox = ImageOps.invert(ink).getbbox()
        part = part.crop(bbox) if bbox else part
        scale = min(178 / part.width, 102 / part.height)
        resized = part.resize(
            (max(1, round(part.width * scale)), max(1, round(part.height * scale))),
            Image.Resampling.LANCZOS,
        )
        resized = ImageOps.autocontrast(resized)
        resized = ImageEnhance.Contrast(resized).enhance(1.8)
        frame = Image.new("L", (184, 108), 255)
        x = (184 - resized.width) // 2
        y = 108 - resized.height - 2
        frame.paste(resized, (x, y))
        prepared.append(frame.point(lambda value: 0 if value < 180 else 255))
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


def clean_species_scene_frame(animal: str, action: str, frame_number: int, frame: Image.Image) -> Image.Image:
    cleaned = frame.copy()
    draw = ImageDraw.Draw(cleaned)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(cleaned):
        tiny_top_speck = max_y <= 15 and max_x - min_x <= 12 and max_y - min_y <= 12
        if count <= 4 or tiny_top_speck:
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    if action == "feed" and animal == "panda" and frame_number == 1:
        draw.rectangle((47, 82, 79, 103), fill=255)
        draw.polygon(((45, 91), (77, 84), (80, 91), (48, 99)), fill=255, outline=0)
        draw.line((56, 89, 59, 96), fill=0, width=1)
        draw.line((67, 87, 70, 93), fill=0, width=1)
        draw.rectangle((47, 100, 83, 107), fill=255)
    if action == "water" and animal == "chicken" and frame_number < 3:
        draw.rectangle((0, 0, 183, 20), fill=255)
    return cleaned


def blink_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    pose = bitmap.copy()
    draw = ImageDraw.Draw(pose)
    radius = spec.get("eye_radius", 4)
    for x, y in spec["eyes"]:
        draw.rectangle((x - radius, y - radius, x + radius, y + radius), fill=255)
        draw.arc((x - radius, y - 2, x + radius, y + 5), 195, 345, fill=0, width=1)
    return pose


def eat_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    # A slight forward lean makes the head visibly dip toward a bowl on the left.
    tilted = bitmap.rotate(8, resample=Image.Resampling.BICUBIC, expand=False, fillcolor=255)
    tilted = tilted.point(lambda value: 0 if value < 170 else 255)
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


def append_companion(output: list[str], image: Image.Image, name: str, spec: dict) -> None:
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


def format_array(name: str, values: list[int]) -> str:
    lines = []
    for start in range(0, len(values), 12):
        chunk = ", ".join(f"0x{value:02X}" for value in values[start:start + 12])
        lines.append(f"  {chunk},")
    return f"const uint8_t {name}[] BITMAP_PROGMEM = {{\n" + "\n".join(lines) + "\n};\n"


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
    final_companions = Image.open(FINAL_COMPANIONS_SOURCE)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    output = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "#if defined(__AVR__)",
        '#define BITMAP_PROGMEM __attribute__((section(".text.zzbitmaps")))',
        "#else",
        "#define BITMAP_PROGMEM PROGMEM",
        "#endif",
        "",
    ]

    for name, spec in SPECS.items():
        append_companion(output, image, name, spec)

    for name, spec in NEW_COMPANION_SPECS.items():
        append_companion(output, new_companions, name, spec)

    for name, spec in FINAL_COMPANION_SPECS.items():
        append_companion(output, final_companions, name, spec)

    output.append("const uint8_t CAT_ACTION_SCENE_WIDTH = 184;")
    output.append("const uint8_t CAT_ACTION_SCENE_HEIGHT = 108;")
    output.append("const uint8_t SPECIES_ACTION_SCENE_WIDTH = 184;")
    output.append("const uint8_t SPECIES_ACTION_SCENE_HEIGHT = 108;")
    for action, scene_sheets in (("feed", SPECIES_FEED_SHEETS), ("water", SPECIES_WATER_SHEETS)):
        for source_path, animals in scene_sheets:
            sheet = Image.open(source_path)
            for row, animal in enumerate(animals):
                contact_sheet = Image.new("L", (184 * 4, 108), 255)
                for frame_number, frame in enumerate(prepare_grid_action_frames(sheet, row, len(animals))):
                    frame = clean_species_scene_frame(animal, action, frame_number, frame)
                    frame.save(PREVIEW_DIR / f"{animal}-{action}-scene-{frame_number}.png")
                    contact_sheet.paste(frame, (184 * frame_number, 0))
                    spans = encode_spans(frame)
                    validate_spans(frame, spans)
                    output.append(format_array(
                        f"{animal.upper()}_{action.upper()}_{frame_number}_SPANS",
                        spans,
                    ))
                contact_sheet.save(PREVIEW_DIR / f"{animal}-{action}-scene-sheet.png")

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
