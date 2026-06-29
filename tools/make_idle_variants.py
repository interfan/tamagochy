from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageEnhance, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE_HEADER = ROOT / "companion_bitmaps.h"
ART_DIR = ROOT / "assets" / "art-candidates"
OUTPUT = ROOT / "animal_idle_variants.h"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
FINAL_DIR = ROOT / "assets" / "pixel-final" / "idle-variants"

ANIMALS = ("cat", "dog", "bunny", "panda", "dragon", "fox", "pig", "hamster", "penguin")
VARIANTS = (("WINK", 1), ("SPARKLE", 2), ("PERK", 3))


def decode_original(text: str, prefix: str) -> Image.Image:
    width = int(re.search(rf"const uint8_t {prefix}_WIDTH = (\d+);", text).group(1))
    height = int(re.search(rf"const uint8_t {prefix}_HEIGHT = (\d+);", text).group(1))
    body = re.search(rf"const uint8_t {prefix}_BITMAP\[\].*?= \{{(.*?)\}};", text, re.S).group(1)
    values = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
    bytes_per_row = (width + 7) // 8
    image = Image.new("L", (width, height), 255)
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            if values[y * bytes_per_row + x // 8] & (0x80 >> (x % 8)):
                pixels[x, y] = 0
    return image


def binarize(image: Image.Image, threshold: int = 185) -> Image.Image:
    gray = ImageOps.autocontrast(image.convert("L"))
    gray = ImageEnhance.Contrast(gray).enhance(1.8)
    return gray.point(lambda value: 0 if value < threshold else 255)


def crop_frame(sheet: Image.Image, frame_index: int) -> Image.Image:
    cell_width = sheet.width // 4
    left = frame_index * cell_width
    right = sheet.width if frame_index == 3 else (frame_index + 1) * cell_width
    margin_x = max(8, cell_width // 18)
    margin_y = max(8, sheet.height // 12)
    return sheet.crop((left + margin_x, margin_y, right - margin_x, sheet.height - margin_y))


def fit_to_original(cell: Image.Image, original: Image.Image) -> Image.Image:
    prepared = binarize(cell)
    bbox = ImageOps.invert(prepared).getbbox()
    canvas = Image.new("L", original.size, 255)
    if not bbox:
        return canvas

    original_bbox = ImageOps.invert(original).getbbox()
    if not original_bbox:
        return canvas
    original_w = original_bbox[2] - original_bbox[0]
    original_h = original_bbox[3] - original_bbox[1]
    original_cx = (original_bbox[0] + original_bbox[2]) // 2
    original_cy = (original_bbox[1] + original_bbox[3]) // 2

    subject = prepared.crop(bbox)
    max_w = min(original.size[0] - 4, original_w + 12)
    max_h = min(original.size[1] - 4, original_h + 12)
    subject.thumbnail((max_w, max_h), Image.Resampling.LANCZOS)
    subject = binarize(subject, 205)
    x = max(0, min(original.size[0] - subject.width, original_cx - subject.width // 2))
    y = max(0, min(original.size[1] - subject.height, original_cy - subject.height // 2))
    canvas.paste(subject, (x, y))
    return canvas


def encode_bitmap(image: Image.Image) -> list[int]:
    width, height = image.size
    bytes_per_row = (width + 7) // 8
    values = []
    pixels = image.load()
    for y in range(height):
        for byte_x in range(bytes_per_row):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width and pixels[x, y] == 0:
                    value |= 0x80 >> bit
            values.append(value)
    return values


def format_array(name: str, values: list[int]) -> str:
    lines = [f"const uint8_t {name}[] BITMAP_PROGMEM = {{"]
    for index in range(0, len(values), 12):
        lines.append("  " + ", ".join(f"0x{value:02X}" for value in values[index:index + 12]) + ",")
    lines.append("};")
    return "\n".join(lines)


def save_variant(animal: str, variant: str, image: Image.Image) -> None:
    filename = f"{animal}-idle-{variant.lower()}.png"
    image.save(FINAL_DIR / filename)
    image.resize((image.width * 2, image.height * 2), Image.Resampling.NEAREST).save(PREVIEW_DIR / filename)


def append_variant(output: list[str], symbol: str, image: Image.Image) -> None:
    output.append(format_array(f"{symbol}_BITMAP", encode_bitmap(image)))
    output.append("")


def make_preview_sheet(frames: list[tuple[str, list[Image.Image]]]) -> None:
    cell_w, cell_h = 132, 156
    sheet = Image.new("L", (cell_w * 4, cell_h * len(frames)), 255)
    draw = ImageDraw.Draw(sheet)
    labels = ("ORIG", "WINK", "SPARKLE", "PERK")
    for row, (animal, images) in enumerate(frames):
        y = row * cell_h
        draw.text((4, y + 2), animal.upper(), fill=0)
        for col, image in enumerate(images):
            x = col * cell_w
            sheet.paste(image, (x + 6, y + 20))
            draw.text((x + 6, y + 140), labels[col], fill=0)
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.Resampling.NEAREST).save(
        PREVIEW_DIR / "idle-art-variants-all-animals.png"
    )


def main() -> None:
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    FINAL_DIR.mkdir(parents=True, exist_ok=True)
    text = SOURCE_HEADER.read_text()
    output = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "#ifndef BITMAP_PROGMEM",
        "#if defined(__AVR__)",
        '#define BITMAP_PROGMEM __attribute__((section(".text.zzbitmaps")))',
        "#else",
        "#define BITMAP_PROGMEM PROGMEM",
        "#endif",
        "#endif",
        "",
    ]
    preview_rows = []

    for animal in ANIMALS:
        prefix = animal.upper()
        original = decode_original(text, prefix)
        sheet_path = ART_DIR / f"{animal}-idle-art-sheet.png"
        if not sheet_path.exists():
            raise FileNotFoundError(sheet_path)
        sheet = Image.open(sheet_path)
        row = [original]
        for variant, frame_index in VARIANTS:
            frame = fit_to_original(crop_frame(sheet, frame_index), original)
            save_variant(animal, variant, frame)
            append_variant(output, f"{prefix}_IDLE_{variant}", frame)
            row.append(frame)
        preview_rows.append((animal, row))

    make_preview_sheet(preview_rows)
    OUTPUT.write_text("\n".join(output), newline="\n")


if __name__ == "__main__":
    main()
