import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


ROOT = Path(__file__).resolve().parents[1]
HEADERS = [
    ROOT / "companion_bitmaps.h",
    ROOT / "animal_idle_variants.h",
    ROOT / "action_icons.h",
    ROOT / "status_bitmaps.h",
    ROOT / "species_action_bitmaps.h",
]
ANIMALS = ("CAT", "DOG", "BUNNY", "PANDA", "DRAGON", "FOX", "PIG", "HAMSTER", "PENGUIN")
MAX_CANVAS = 200


@dataclass(frozen=True)
class BitmapArray:
    header: Path
    name: str
    values: List[int]
    rle: bool


def read_headers() -> str:
    return "\n".join(path.read_text() for path in HEADERS)


def parse_constants(text: str) -> Dict[str, int]:
    constants = {}
    for match in re.finditer(r"const\s+uint(?:8|16)_t\s+(\w+)\s*=\s*(\d+)\s*;", text):
        constants[match.group(1)] = int(match.group(2))
    return constants


def parse_arrays() -> List[BitmapArray]:
    arrays = []
    array_re = re.compile(r"const\s+uint8_t\s+(\w+)\[\]\s+BITMAP_PROGMEM\s*=\s*\{(.*?)\};", re.S)
    for header in HEADERS:
        text = header.read_text()
        for match in array_re.finditer(text):
            values = [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(2))]
            name = match.group(1)
            arrays.append(BitmapArray(header, name, values, name.endswith("_RLE")))
    return arrays


def direct_dimension_name(name: str, suffix: str) -> str:
    return f"{name}_{suffix}"


def dimensions_for(name: str, constants: Dict[str, int]) -> Optional[Tuple[int, int]]:
    direct_width = direct_dimension_name(name, "WIDTH")
    direct_height = direct_dimension_name(name, "HEIGHT")
    if direct_width in constants and direct_height in constants:
        return constants[direct_width], constants[direct_height]

    if name.endswith("_BITMAP"):
        stem = name[:-len("_BITMAP")]
        stem_width = direct_dimension_name(stem, "WIDTH")
        stem_height = direct_dimension_name(stem, "HEIGHT")
        if stem_width in constants and stem_height in constants:
            return constants[stem_width], constants[stem_height]

    if name.startswith("ACTION_") and name.endswith("_ICON_BITMAP"):
        return constants.get("ACTION_ICON_WIDTH"), constants.get("ACTION_ICON_HEIGHT")

    if name.endswith("_IDLE_WINK_BITMAP") or name.endswith("_IDLE_SPARKLE_BITMAP") or name.endswith("_IDLE_PERK_BITMAP"):
        animal = name.split("_IDLE_", 1)[0]
        return constants.get(f"{animal}_WIDTH"), constants.get(f"{animal}_HEIGHT")

    for animal in ANIMALS:
        if name == f"{animal}_BITMAP" or name.startswith(f"{animal}_HAPPY_") or name.startswith(f"{animal}_SLEEP_"):
            return constants.get(f"{animal}_WIDTH"), constants.get(f"{animal}_HEIGHT")

    return None


def rle_dimensions_for(name: str, constants: Dict[str, int]) -> Optional[Tuple[int, int]]:
    if name.startswith("CAT_"):
        return constants.get("CAT_ACTION_SCENE_WIDTH"), constants.get("CAT_ACTION_SCENE_HEIGHT")
    return constants.get("SPECIES_ACTION_SCENE_WIDTH"), constants.get("SPECIES_ACTION_SCENE_HEIGHT")


def packed_bbox(values: List[int], width: int, height: int) -> Optional[Tuple[int, int, int, int]]:
    min_x, min_y = width, height
    max_x, max_y = -1, -1
    bytes_per_row = (width + 7) // 8
    for y in range(height):
        for x in range(width):
            value = values[y * bytes_per_row + x // 8]
            if value & (0x80 >> (x % 8)):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < 0:
        return None
    return min_x, min_y, max_x + 1, max_y + 1


def validate_packed(array: BitmapArray, width: int, height: int) -> Tuple[List[str], List[str]]:
    errors = []
    warnings = []
    expected = ((width + 7) // 8) * height
    if len(array.values) != expected:
        errors.append(f"{array.name}: {len(array.values)} bytes, expected {expected} for {width}x{height}")
        return errors, warnings
    if width > MAX_CANVAS or height > MAX_CANVAS:
        errors.append(f"{array.name}: {width}x{height} exceeds {MAX_CANVAS}x{MAX_CANVAS}")
    bbox = packed_bbox(array.values, width, height)
    if not bbox:
        errors.append(f"{array.name}: empty bitmap")
        return errors, warnings
    left, top, right, bottom = bbox
    bbox_w = right - left
    bbox_h = bottom - top
    if bbox_w < 6 or bbox_h < 6:
        warnings.append(f"{array.name}: very small visible area {bbox_w}x{bbox_h}")
    if left == 0 or top == 0 or right == width or bottom == height:
        warnings.append(f"{array.name}: visible pixels touch canvas edge")
    return errors, warnings


def validate_rle(array: BitmapArray, width: int, height: int) -> Tuple[List[str], List[str]]:
    errors = []
    warnings = []
    total = 0
    black = False
    black_pixels = 0
    for value in array.values:
        if value == 0:
            black = not black
            continue
        total += value
        if black:
            black_pixels += value
        black = not black
    expected = width * height
    if total != expected:
        errors.append(f"{array.name}: RLE expands to {total} pixels, expected {expected}")
    if black_pixels == 0:
        errors.append(f"{array.name}: RLE has no black pixels")
    if width > MAX_CANVAS or height > MAX_CANVAS:
        errors.append(f"{array.name}: {width}x{height} exceeds {MAX_CANVAS}x{MAX_CANVAS}")
    return errors, warnings


def main() -> int:
    missing = [str(path) for path in HEADERS if not path.exists()]
    if missing:
        print("Bitmap check failed: missing headers:")
        for path in missing:
            print(f"  {path}")
        return 1

    text = read_headers()
    constants = parse_constants(text)
    arrays = parse_arrays()
    errors = []
    warnings = []

    for array in arrays:
        dims = rle_dimensions_for(array.name, constants) if array.rle else dimensions_for(array.name, constants)
        if not dims or dims[0] is None or dims[1] is None:
            errors.append(f"{array.name}: could not resolve dimensions")
            continue
        width, height = dims
        if array.rle:
            item_errors, item_warnings = validate_rle(array, width, height)
        else:
            item_errors, item_warnings = validate_packed(array, width, height)
        errors.extend(f"{array.header.name}: {message}" for message in item_errors)
        warnings.extend(f"{array.header.name}: {message}" for message in item_warnings)

    if warnings:
        print("Bitmap check warnings:")
        for warning in warnings[:25]:
            print(f"  {warning}")
        if len(warnings) > 25:
            print(f"  ... {len(warnings) - 25} more warnings")

    if errors:
        print("Bitmap check failed:")
        for error in errors:
            print(f"  {error}")
        return 1

    print(f"Bitmap check passed: {len(arrays)} arrays validated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
