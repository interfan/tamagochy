from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "status" / "generated-status-sheet.png"
OUTPUT = ROOT / "status_bitmaps.h"
FINAL_DIR = ROOT / "assets" / "pixel-final" / "status"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"

ASSETS = {
    "hospital": {"quad": (0, 0), "size": (120, 96), "max": (112, 90), "symbol": "HOSPITAL_STATUS"},
    "virus": {"quad": (1, 0), "size": (28, 28), "max": (26, 26), "symbol": "VIRUS_STATUS"},
    "poop": {"quad": (0, 1), "size": (28, 24), "max": (26, 22), "symbol": "POOP_STATUS"},
}


def binary(image: Image.Image, threshold: int = 185) -> Image.Image:
    gray = ImageOps.autocontrast(image.convert("L"))
    gray = ImageEnhance.Contrast(gray).enhance(1.6)
    return gray.point(lambda value: 0 if value < threshold else 255)


def quadrant(source: Image.Image, col: int, row: int) -> Image.Image:
    half_w = source.width // 2
    half_h = source.height // 2
    margin = min(32, half_w // 12, half_h // 12)
    left = col * half_w + margin
    top = row * half_h + margin
    right = (col + 1) * half_w - margin
    bottom = (row + 1) * half_h - margin
    return source.crop((left, top, right, bottom))


def fit_ink(image: Image.Image, frame_size: tuple[int, int], max_size: tuple[int, int]) -> Image.Image:
    prepared = binary(image)
    bbox = ImageOps.invert(prepared).getbbox()
    canvas = Image.new("L", frame_size, 255)
    if not bbox:
        return canvas
    subject = prepared.crop(bbox)
    subject.thumbnail(max_size, Image.Resampling.LANCZOS)
    subject = binary(subject, 205)
    x = (frame_size[0] - subject.width) // 2
    y = (frame_size[1] - subject.height) // 2
    canvas.paste(subject, (x, y))
    return canvas


def components(image: Image.Image) -> list[tuple[int, int, int, int]]:
    pixels = image.load()
    width, height = image.size
    seen = set()
    boxes = []
    for y in range(height):
        for x in range(width):
            if (x, y) in seen or pixels[x, y] != 0:
                continue
            stack = [(x, y)]
            seen.add((x, y))
            xs = []
            ys = []
            while stack:
                px, py = stack.pop()
                xs.append(px)
                ys.append(py)
                for nx, ny in ((px + 1, py), (px - 1, py), (px, py + 1), (px, py - 1)):
                    if 0 <= nx < width and 0 <= ny < height and (nx, ny) not in seen and pixels[nx, ny] == 0:
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if len(xs) > 20:
                boxes.append((min(xs), min(ys), max(xs) + 1, max(ys) + 1))
    return boxes


def prepare_smell(source: Image.Image) -> Image.Image:
    smell_quad = binary(quadrant(source, 1, 1))
    boxes = sorted(components(smell_quad), key=lambda box: box[0])
    if not boxes:
        return Image.new("L", (14, 36), 255)
    box = boxes[len(boxes) // 2]
    pad = 3
    box = (
        max(0, box[0] - pad),
        max(0, box[1] - pad),
        min(smell_quad.width, box[2] + pad),
        min(smell_quad.height, box[3] + pad),
    )
    return fit_ink(smell_quad.crop(box), (14, 36), (12, 34))


def encode_bitmap(image: Image.Image) -> list[int]:
    width, height = image.size
    bytes_per_row = (width + 7) // 8
    data = []
    pixels = image.load()
    for y in range(height):
        for byte_x in range(bytes_per_row):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width and pixels[x, y] == 0:
                    value |= 0x80 >> bit
            data.append(value)
    return data


def format_array(name: str, data: list[int]) -> str:
    lines = [f"const uint8_t {name}[] BITMAP_PROGMEM = {{"]
    for index in range(0, len(data), 12):
        lines.append("  " + ", ".join(f"0x{value:02X}" for value in data[index:index + 12]) + ",")
    lines.append("};")
    return "\n".join(lines)


def append_asset(output: list[str], name: str, image: Image.Image) -> None:
    symbol = name.upper()
    width, height = image.size
    output.append(f"const uint8_t {symbol}_BITMAP_WIDTH = {width};")
    output.append(f"const uint8_t {symbol}_BITMAP_HEIGHT = {height};")
    output.append(format_array(f"{symbol}_BITMAP", encode_bitmap(image)))
    output.append("")
    image.save(FINAL_DIR / f"{name.lower()}.png")
    image.resize((width * 3, height * 3), Image.Resampling.NEAREST).save(
        PREVIEW_DIR / f"status-{name.lower()}.png"
    )


def main() -> None:
    FINAL_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    source = Image.open(SOURCE)

    rendered: dict[str, Image.Image] = {}
    for name, spec in ASSETS.items():
        rendered[spec["symbol"]] = fit_ink(
            quadrant(source, *spec["quad"]),
            spec["size"],
            spec["max"],
        )
    rendered["SMELL_STATUS"] = prepare_smell(source)

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

    for symbol, image in rendered.items():
        append_asset(output, symbol, image)

    OUTPUT.write_text("\n".join(output), newline="\n")


if __name__ == "__main__":
    main()
