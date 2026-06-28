from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

from make_companion_bitmaps import (
    clean_species_scene_frame,
    connected_components,
    encode_rle,
    format_array,
    load_pixel_action_frames,
    validate_rle,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "assets" / "action-scenes"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
OUTPUT = ROOT / "species_action_bitmaps.h"

COMPANION_ANIMALS = ("dog", "bunny", "panda", "pig", "hamster")
SPECIAL_ANIMALS = ("dragon", "fox", "penguin")
ACTIONS = ("medicine", "pet", "groom", "clean", "wash", "learn")
ACTION_SCENE_WIDTH = 184
ACTION_SCENE_HEIGHT = 184
ACTION_SCENE_PADDING = 4
ACTION_SCENE_MAX_SCALE_BOOST = 1.8
ACTION_SUBJECT_TARGET = 132
ACTION_FINAL_MIN_SIZE = 112
ACTION_LINE_THRESHOLD = 176


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
        top_read_fragment = action == "learn" and max_y <= 55
        if top_read_fragment or (touches_outer_edge and count < 100):
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    masks = {
        ("pig", "learn", 3): ((0, 0, 183, 58),),
    }
    for rectangle in masks.get((animal, action, frame_number), ()):
        draw.rectangle(rectangle, fill=255)
    return cleaned


def finish_action_frame(frame: Image.Image) -> Image.Image:
    ink = remove_isolated_noise(binarize_action_frame(frame))
    bbox = ImageOps.invert(ink).getbbox()
    if not bbox:
        return ink
    cropped = frame.crop(bbox)
    max_dimension = max(cropped.width, cropped.height)
    if max_dimension < ACTION_FINAL_MIN_SIZE:
        scale = min(
            (ACTION_SCENE_WIDTH - ACTION_SCENE_PADDING) / cropped.width,
            (ACTION_SCENE_HEIGHT - ACTION_SCENE_PADDING) / cropped.height,
            ACTION_FINAL_MIN_SIZE / max_dimension,
        )
        cropped = cropped.resize(
            (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
            Image.Resampling.LANCZOS,
        )
    canvas = Image.new("L", (ACTION_SCENE_WIDTH, ACTION_SCENE_HEIGHT), 255)
    canvas.paste(
        cropped,
        ((ACTION_SCENE_WIDTH - cropped.width) // 2, (ACTION_SCENE_HEIGHT - cropped.height) // 2),
    )
    return remove_isolated_noise(binarize_action_frame(canvas))


def subject_bbox(frame: Image.Image) -> tuple[int, int, int, int] | None:
    ink = frame.point(lambda value: 0 if value < 190 else 255)
    best = None
    best_score = None
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(ink):
        width = max_x - min_x
        height = max_y - min_y
        if count < 25 or width < 6 or height < 6:
            continue
        center_y = (min_y + max_y) / 2
        too_high = max_y < frame.height * 0.25
        edge_penalty = 120 if (min_x <= 2 or max_x >= frame.width - 2) else 0
        score = count + width * height * 0.04 + center_y * 0.45 - abs(center_x - frame.width / 2) * 0.55
        if too_high:
            score -= 500
        score -= edge_penalty
        if best_score is None or score > best_score:
            best_score = score
            best = (min_x, min_y, max_x, max_y)
    return best


def action_frame_scale(frame: Image.Image, base_scale: float) -> float:
    fit_scale = min(
        (ACTION_SCENE_WIDTH - ACTION_SCENE_PADDING) / frame.width,
        (ACTION_SCENE_HEIGHT - ACTION_SCENE_PADDING) / frame.height,
    )
    subject = subject_bbox(frame)
    if not subject:
        return min(fit_scale, base_scale * ACTION_SCENE_MAX_SCALE_BOOST)
    subject_width = subject[2] - subject[0]
    subject_height = subject[3] - subject[1]
    subject_scale = ACTION_SUBJECT_TARGET / max(subject_width, subject_height)
    return min(fit_scale, subject_scale, base_scale * ACTION_SCENE_MAX_SCALE_BOOST)


def binarize_action_frame(frame: Image.Image) -> Image.Image:
    smoothed = ImageOps.autocontrast(frame)
    smoothed = ImageEnhance.Contrast(smoothed).enhance(1.35)
    smoothed = smoothed.filter(ImageFilter.SMOOTH)
    return smoothed.point(lambda value: 0 if value < ACTION_LINE_THRESHOLD else 255)


def remove_isolated_noise(frame: Image.Image) -> Image.Image:
    subject = subject_bbox(frame)
    if not subject:
        return frame
    subject_top = subject[1]
    subject_bottom = subject[3]
    cleaned = frame.copy()
    draw = ImageDraw.Draw(cleaned)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(frame):
        width = max_x - min_x
        height = max_y - min_y
        tiny = count <= 12 and width <= 5 and height <= 5
        outside_subject_y = max_y < subject_top - 4 or min_y > subject_bottom + 4
        if tiny and outside_subject_y:
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
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
    scale = min(
        (ACTION_SCENE_WIDTH - ACTION_SCENE_PADDING) / max_width,
        (ACTION_SCENE_HEIGHT - ACTION_SCENE_PADDING) / max_height,
    )
    prepared = []
    for frame in cropped:
        frame_scale = action_frame_scale(frame, scale)
        resized = frame.resize(
            (max(1, round(frame.width * frame_scale)), max(1, round(frame.height * frame_scale))),
            Image.Resampling.LANCZOS,
        )
        resized = ImageOps.autocontrast(resized)
        resized = ImageEnhance.Contrast(resized).enhance(1.8)
        canvas = Image.new("L", (ACTION_SCENE_WIDTH, ACTION_SCENE_HEIGHT), 255)
        canvas.paste(
            resized,
            ((ACTION_SCENE_WIDTH - resized.width) // 2, (ACTION_SCENE_HEIGHT - resized.height) // 2),
        )
        prepared.append(canvas)
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
                    pixel_frames = load_pixel_action_frames(animal, action)
                    frames = pixel_frames or prepare_action_row(source_sheet, row, len(animals), action)
                    preview_sheet = Image.new("L", (ACTION_SCENE_WIDTH * 4, ACTION_SCENE_HEIGHT), 255)
                    for frame_number, frame in enumerate(frames):
                        if pixel_frames is None:
                            frame = clean_species_scene_frame(animal, action, frame_number, frame)
                            frame = clean_final_frame(animal, action, frame_number, frame)
                            frame = finish_action_frame(frame)
                        frame.save(PREVIEW_DIR / f"{animal}-{action}-scene-{frame_number}.png")
                        preview_sheet.paste(frame, (ACTION_SCENE_WIDTH * frame_number, 0))

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
