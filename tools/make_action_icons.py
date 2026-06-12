from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "action-icons.png"
OUTPUT = ROOT / "action_icons.h"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
ICON_WIDTH = 28
ICON_HEIGHT = 24

ICON_CROPS = {
    "food": (42, 375, 198, 510),
    "water": (218, 375, 330, 510),
    "bed": (345, 375, 490, 510),
    "moon": (500, 375, 630, 510),
    "overnight": (630, 375, 765, 510),
    "clean": (780, 375, 925, 510),
    "wash": (935, 375, 1050, 510),
    "learn": (1060, 375, 1225, 510),
    "pet": (1230, 375, 1370, 510),
    "medicine": (1370, 375, 1490, 510),
    "play": (1495, 365, 1610, 520),
    "groom": (1615, 365, 1720, 520),
}


def prepare_icon(source: Image.Image, crop: tuple[int, int, int, int]) -> Image.Image:
    icon = ImageOps.autocontrast(source.crop(crop).convert("L"))
    ink = icon.point(lambda value: 0 if value < 220 else 255)
    bbox = ImageOps.invert(ink).getbbox()
    if not bbox:
        raise ValueError(f"No icon found in crop {crop}")
    icon = icon.crop(bbox)
    icon.thumbnail((ICON_WIDTH - 2, ICON_HEIGHT - 2), Image.Resampling.LANCZOS)
    icon = icon.point(lambda value: 0 if value < 150 else 255)

    canvas = Image.new("L", (ICON_WIDTH, ICON_HEIGHT), 255)
    canvas.paste(icon, ((ICON_WIDTH - icon.width) // 2, (ICON_HEIGHT - icon.height) // 2))
    return canvas


def encode_bitmap(image: Image.Image) -> list[int]:
    values = []
    pixels = image.load()
    for y in range(image.height):
        for byte_x in range((image.width + 7) // 8):
            value = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < image.width and pixels[x, y] == 0:
                    value |= 0x80 >> bit
            values.append(value)
    return values


def format_array(name: str, values: list[int]) -> str:
    lines = []
    for start in range(0, len(values), 12):
        chunk = ", ".join(f"0x{value:02X}" for value in values[start:start + 12])
        lines.append(f"  {chunk},")
    return f"const uint8_t {name}[] BITMAP_PROGMEM = {{\n" + "\n".join(lines) + "\n};\n"


def main() -> None:
    source = Image.open(SOURCE)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    contact_sheet = Image.new("L", (ICON_WIDTH * len(ICON_CROPS), ICON_HEIGHT), 255)
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
        f"const uint8_t ACTION_ICON_WIDTH = {ICON_WIDTH};",
        f"const uint8_t ACTION_ICON_HEIGHT = {ICON_HEIGHT};",
        "",
    ]

    for index, (name, crop) in enumerate(ICON_CROPS.items()):
        icon = prepare_icon(source, crop)
        icon.save(PREVIEW_DIR / f"action-icon-{name}.png")
        contact_sheet.paste(icon, (index * ICON_WIDTH, 0))
        output.append(format_array(f"ACTION_{name.upper()}_ICON_BITMAP", encode_bitmap(icon)))

    contact_sheet.resize(
        (contact_sheet.width * 4, contact_sheet.height * 4),
        Image.Resampling.NEAREST,
    ).save(PREVIEW_DIR / "action-icons-sheet.png")
    OUTPUT.write_text("\n".join(output), encoding="ascii")


if __name__ == "__main__":
    main()
