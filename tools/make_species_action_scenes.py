from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageOps

from make_companion_bitmaps import (
    clean_species_scene_frame,
    connected_components,
    encode_rle,
    format_array,
    validate_rle,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "action-scenes"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
OUTPUT = ROOT / "species_action_bitmaps.h"

COMPANION_ANIMALS = ("dog", "bunny", "panda", "chicken", "pig", "hamster")
SPECIAL_ANIMALS = ("dragon", "fox", "penguin")
ACTIONS = ("medicine", "pet", "groom", "clean", "wash", "learn")


def action_sources(action: str) -> tuple[tuple[Path, tuple[str, ...]], ...]:
    return (
        (SOURCE_DIR / f"species-{action}-companions.png", COMPANION_ANIMALS),
        (SOURCE_DIR / f"species-{action}-dragon-fox-penguin.png", SPECIAL_ANIMALS),
    )


def row_window(
    row: int,
    row_count: int,
    height: int,
    action: str,
    column: int,
) -> tuple[int, int]:
    layouts = {
        6: ((0, 220), (180, 405), (350, 580), (520, 740), (675, 900), (830, 1024)),
        3: ((0, 390), (300, 730), (630, 1024)),
    }
    start, end = layouts[row_count][row]
    learn_overrides = {
        (1, 1): (180, 380),
        (1, 2): (180, 380),
        (1, 3): (205, 405),
        (2, 1): (380, 580),
        (2, 2): (380, 580),
        (3, 3): (520, 700),
        (4, 3): (710, 900),
    }
    if action == "learn" and row_count == 6:
        start, end = learn_overrides.get((row, column), (start, end))
    return round(start * height / 1024), round(end * height / 1024)


def column_window(column: int, width: int) -> tuple[int, int]:
    windows = ((0, 420), (340, 805), (720, 1190), (1110, 1536))
    start, end = windows[column]
    return round(start * width / 1536), round(end * width / 1536)


def remove_edge_leaks(part: Image.Image) -> Image.Image:
    cleaned = part.copy()
    draw = ImageDraw.Draw(cleaned)
    binary = part.point(lambda value: 0 if value < 180 else 255)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(binary):
        center_y = (min_y + max_y) / 2
        component_center_x = (min_x + max_x) / 2
        outside_y = center_y < part.height * 0.25 or center_y > part.height * 0.75
        outside_x = component_center_x < part.width * 0.18 or component_center_x > part.width * 0.82
        touches_y = min_y <= 1 or max_y >= part.height - 1
        touches_x = min_x <= 1 or max_x >= part.width - 1
        if (touches_y and outside_y) or (touches_x and outside_x):
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    return cleaned


def clean_final_frame(animal: str, action: str, frame_number: int, frame: Image.Image) -> Image.Image:
    cleaned = frame.copy()
    draw = ImageDraw.Draw(cleaned)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(cleaned):
        touches_outer_edge = min_y <= 1 or min_x <= 1 or max_x >= frame.width - 1
        if touches_outer_edge and count < 100:
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    masks = {
        ("dog", "pet", 2): ((0, 0, 14, 107),),
        ("panda", "learn", 1): ((0, 0, 183, 12),),
        ("panda", "learn", 2): ((0, 0, 183, 12),),
        ("pig", "learn", 3): ((0, 0, 183, 15),),
        ("penguin", "clean", 0): ((0, 0, 183, 10),),
    }
    for rectangle in masks.get((animal, action, frame_number), ()):
        draw.rectangle(rectangle, fill=255)
    return cleaned


def prepare_action_row(
    source_sheet: Image.Image,
    row: int,
    row_count: int,
    action: str,
) -> list[Image.Image]:
    rgba = source_sheet.convert("RGBA")
    white = Image.new("RGBA", rgba.size, "white")
    white.alpha_composite(rgba)
    source = ImageOps.autocontrast(white.convert("L"))

    cropped = []
    for column in range(4):
        start_x, end_x = column_window(column, source.width)
        start_y, end_y = row_window(row, row_count, source.height, action, column)
        part = source.crop((
            start_x,
            start_y,
            end_x,
            end_y,
        ))
        part = remove_edge_leaks(part)
        ink = part.point(lambda value: 0 if value < 225 else 255)
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
        resized = ImageEnhance.Contrast(resized).enhance(1.8)
        canvas = Image.new("L", (184, 108), 255)
        canvas.paste(resized, ((184 - resized.width) // 2, 108 - resized.height - 2))
        prepared.append(canvas.point(lambda value: 0 if value < 180 else 255))
    return prepared


def main() -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    output = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "#ifndef BITMAP_PROGMEM",
        "#define BITMAP_PROGMEM PROGMEM",
        "#endif",
        "",
    ]
    frame_count = 0

    for action in ACTIONS:
        for source, animals in action_sources(action):
            with Image.open(source) as source_sheet:
                for row, animal in enumerate(animals):
                    frames = prepare_action_row(source_sheet, row, len(animals), action)
                    preview_sheet = Image.new("L", (184 * 4, 108), 255)
                    for frame_number, frame in enumerate(frames):
                        frame = clean_species_scene_frame(animal, action, frame_number, frame)
                        frame = clean_final_frame(animal, action, frame_number, frame)
                        frame.save(PREVIEW_DIR / f"{animal}-{action}-scene-{frame_number}.png")
                        preview_sheet.paste(frame, (184 * frame_number, 0))

                        runs = encode_rle(frame)
                        validate_rle(frame, runs)
                        output.append(format_array(
                            f"{animal.upper()}_{action.upper()}_{frame_number}_RLE",
                            runs,
                        ))
                        frame_count += 1
                    preview_sheet.save(PREVIEW_DIR / f"{animal}-{action}-scene-sheet.png")

    OUTPUT.write_text("\n".join(output), encoding="ascii")
    print(f"Wrote {frame_count} illustrated action frames to {OUTPUT.name}")


if __name__ == "__main__":
    main()
