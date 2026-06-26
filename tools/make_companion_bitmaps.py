from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "kawaii-companion-concepts.png"
NEW_COMPANIONS_SOURCE = ROOT / "assets" / "kawaii-new-companions.png"
FINAL_COMPANIONS_SOURCE = ROOT / "assets" / "kawaii-final-companions.png"
EGG_SOURCE = ROOT / "assets" / "kawaii-egg-concept.png"
OUTPUT = ROOT / "companion_bitmaps.h"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
PIXEL_FINAL_DIR = ROOT / "assets" / "pixel-final"
PIXEL_ANIMAL_DIR = PIXEL_FINAL_DIR / "animals"
PIXEL_ACTION_SCENE_DIR = PIXEL_FINAL_DIR / "action-scenes"
ACTION_SCENE_DIR = ROOT / "assets" / "action-scenes"
SPECIES_FEED_COMPANIONS_SOURCE = ACTION_SCENE_DIR / "species-feed-companions.png"
SPECIES_FEED_SPECIAL_SOURCE = ACTION_SCENE_DIR / "species-feed-dragon-fox-penguin.png"
SPECIES_WATER_COMPANIONS_SOURCE = ACTION_SCENE_DIR / "species-water-companions.png"
SPECIES_WATER_SPECIAL_SOURCE = ACTION_SCENE_DIR / "species-water-dragon-fox-penguin.png"
SPECIES_SLEEP_COMPANIONS_SOURCE = ACTION_SCENE_DIR / "species-sleep-companions.png"
SPECIES_SLEEP_SPECIAL_SOURCE = ACTION_SCENE_DIR / "species-sleep-dragon-fox-penguin.png"
ACTION_SCENES = (
    "feed", "water", "sleep", "overnight", "clean",
    "medicine", "learn", "pet", "groom", "wash",
)
ACTION_SCENE_WIDTH = 184
ACTION_SCENE_HEIGHT = 184
ACTION_SCENE_PADDING = 4
ACTION_SCENE_MAX_SCALE_BOOST = 1.8
ACTION_SUBJECT_TARGET = 132
ACTION_FINAL_MIN_SIZE = 112
ACTION_LINE_THRESHOLD = 176
SPECS = {
    "cat": {
        "crop": (0, 100, 570, 770), "width": 88, "height": 96, "threshold": 170,
        "eyes": [(39, 38), (55, 38)], "mouth": (47, 51),
    },
    "dog": {
        "crop": (585, 155, 1145, 770), "width": 96, "height": 100, "threshold": 158,
        "eyes": [(41, 44), (65, 43)], "mouth": (54, 58),
    },
    "bunny": {
        "crop": (1190, 130, 1745, 770), "width": 96, "height": 100, "threshold": 165,
        "eyes": [(46, 51), (69, 50)], "mouth": (58, 63),
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
SPECIES_SLEEP_SHEETS = (
    (SPECIES_SLEEP_COMPANIONS_SOURCE, ("dog", "bunny", "panda", "chicken", "pig", "hamster")),
    (SPECIES_SLEEP_SPECIAL_SOURCE, ("dragon", "fox", "penguin")),
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


def normalize_pixel_final(image: Image.Image, size: tuple[int, int], path: Path) -> Image.Image:
    if image.size != size:
        raise ValueError(f"{path} must be {size[0]}x{size[1]}, got {image.size[0]}x{image.size[1]}")
    return image.convert("L").point(lambda value: 0 if value < 128 else 255)


def load_pixel_final(path: Path, size: tuple[int, int]) -> Image.Image | None:
    if not path.exists():
        return None
    with Image.open(path) as image:
        return normalize_pixel_final(image, size, path)


def load_pixel_final_any_size(path: Path) -> Image.Image | None:
    if not path.exists():
        return None
    with Image.open(path) as image:
        return normalize_pixel_final(image, image.size, path)


def load_pixel_animal(name: str, size: tuple[int, int] | None = None, pose: str | None = None) -> Image.Image | None:
    suffix = f"-{pose}" if pose else ""
    path = PIXEL_ANIMAL_DIR / f"{name}{suffix}.png"
    if not path.exists():
        return None
    with Image.open(path) as image:
        expected_size = size if size is not None else image.size
        return normalize_pixel_final(image, expected_size, path)


def load_pixel_action_frames(animal: str, action: str) -> list[Image.Image] | None:
    paths = [PIXEL_ACTION_SCENE_DIR / f"{animal}-{action}-{frame}.png" for frame in range(4)]
    if not all(path.exists() for path in paths):
        return None
    return [load_pixel_final(path, (ACTION_SCENE_WIDTH, ACTION_SCENE_HEIGHT)) for path in paths]


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
        resized = ImageEnhance.Contrast(resized).enhance(2.0)
        canvas = Image.new("L", (ACTION_SCENE_WIDTH, ACTION_SCENE_HEIGHT), 255)
        x = (ACTION_SCENE_WIDTH - resized.width) // 2
        y = (ACTION_SCENE_HEIGHT - resized.height) // 2
        canvas.paste(resized, (x, y))
        prepared.append(canvas)
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
        prepared.append(part)

    max_width = max(part.width for part in prepared)
    max_height = max(part.height for part in prepared)
    scale = min(
        (ACTION_SCENE_WIDTH - ACTION_SCENE_PADDING) / max_width,
        (ACTION_SCENE_HEIGHT - ACTION_SCENE_PADDING) / max_height,
    )
    centered = []
    for part in prepared:
        frame_scale = action_frame_scale(part, scale)
        resized = part.resize(
            (max(1, round(part.width * frame_scale)), max(1, round(part.height * frame_scale))),
            Image.Resampling.LANCZOS,
        )
        resized = ImageOps.autocontrast(resized)
        resized = ImageEnhance.Contrast(resized).enhance(1.8)
        frame = Image.new("L", (ACTION_SCENE_WIDTH, ACTION_SCENE_HEIGHT), 255)
        x = (ACTION_SCENE_WIDTH - resized.width) // 2
        y = (ACTION_SCENE_HEIGHT - resized.height) // 2
        frame.paste(resized, (x, y))
        centered.append(frame)
    return centered


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
    canvas.paste(cropped, ((ACTION_SCENE_WIDTH - cropped.width) // 2,
                           (ACTION_SCENE_HEIGHT - cropped.height) // 2))
    return remove_isolated_noise(binarize_action_frame(canvas))


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
    if action == "sleep" and frame_number < 3:
        clear_top = animal in ("panda", "bunny") or \
            (animal in ("pig", "chicken") and frame_number > 0)
        if clear_top:
            draw.rectangle((0, 0, 183, 25), fill=255)
    if action == "sleep" and animal in ("panda", "bunny"):
        draw.rectangle((0, 0, 150, 25), fill=255)
        for min_x, min_y, max_x, max_y, center_x, count in connected_components(cleaned):
            if max_x <= 150 and max_y <= 35 and count <= 35:
                draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    if action == "sleep" and animal == "bunny" and frame_number >= 2:
        draw.arc((36, 10, 148, 116), 180, 360, fill=0, width=2)
    return cleaned


def blink_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    pose = bitmap.copy()
    draw = ImageDraw.Draw(pose)
    scale_x = bitmap.width / spec["width"]
    scale_y = bitmap.height / spec["height"]
    radius = max(1, round(spec.get("eye_radius", 4) * min(scale_x, scale_y)))
    for x, y in spec["eyes"]:
        x = round(x * scale_x)
        y = round(y * scale_y)
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
    scale_x = bitmap.width / spec["width"]
    scale_y = bitmap.height / spec["height"]
    mx, my = round(spec["mouth"][0] * scale_x), round(spec["mouth"][1] * scale_y)
    draw.rectangle((mx - 5, my - 4, mx + 5, my + 5), fill=255)
    draw.arc((mx - 5, my - 5, mx + 5, my + 5), 15, 165, fill=0, width=2)
    if animal == "cat":
        draw.rectangle((0, round(51 * scale_y), round(30 * scale_x), round(80 * scale_y)), fill=255)
        draw.arc((round(2 * scale_x), round(45 * scale_y), round(27 * scale_x), round(70 * scale_y)), 80, 285, fill=0, width=2)
        draw.arc((round(7 * scale_x), round(50 * scale_y), round(22 * scale_x), round(65 * scale_y)), 80, 285, fill=0, width=1)
        draw.line((round(22 * scale_x), round(64 * scale_y), round(31 * scale_x), round(72 * scale_y)), fill=0, width=2)
    return pose


def sleep_pose(bitmap: Image.Image, spec: dict) -> Image.Image:
    pose = blink_pose(bitmap, spec)
    draw = ImageDraw.Draw(pose)
    scale_x = bitmap.width / spec["width"]
    scale_y = bitmap.height / spec["height"]
    mx, my = round(spec["mouth"][0] * scale_x), round(spec["mouth"][1] * scale_y)
    draw.rectangle((mx - 5, my - 4, mx + 5, my + 5), fill=255)
    draw.ellipse((mx - 3, my - 2, mx + 3, my + 4), outline=0, width=2)
    return pose


def prepare_companion_from_scene(scene: Image.Image, width: int, height: int) -> Image.Image:
    source = scene.convert("L").point(lambda value: 0 if value < 180 else 255)
    pixels = source.load()
    seen = bytearray(source.width * source.height)
    components = []
    for y in range(source.height):
        for x in range(source.width):
            index = y * source.width + x
            if pixels[x, y] != 0 or seen[index]:
                continue
            stack = [(x, y)]
            seen[index] = 1
            points = []
            while stack:
                px, py = stack.pop()
                points.append((px, py))
                for nx, ny in ((px - 1, py), (px + 1, py), (px, py - 1), (px, py + 1)):
                    if not (0 <= nx < source.width and 0 <= ny < source.height):
                        continue
                    neighbor = ny * source.width + nx
                    if pixels[nx, ny] == 0 and not seen[neighbor]:
                        seen[neighbor] = 1
                        stack.append((nx, ny))
            components.append(points)

    body = max(components, key=len)
    body_box = (
        min(x for x, y in body),
        min(y for x, y in body),
        max(x for x, y in body) + 1,
        max(y for x, y in body) + 1,
    )
    animal = Image.new("L", source.size, 255)
    animal_pixels = animal.load()
    for component in components:
        box = (
            min(x for x, y in component),
            min(y for x, y in component),
            max(x for x, y in component) + 1,
            max(y for x, y in component) + 1,
        )
        if (body_box[0] <= box[0] and body_box[1] <= box[1] and
                box[2] <= body_box[2] and box[3] <= body_box[3]):
            for x, y in component:
                animal_pixels[x, y] = 0

    animal = animal.crop(body_box)
    animal.thumbnail((width - 2, height - 2), Image.Resampling.LANCZOS)
    animal = animal.point(lambda value: 0 if value < 180 else 255)
    canvas = Image.new("L", (width, height), 255)
    canvas.paste(animal, ((width - animal.width) // 2, height - animal.height - 1))
    return canvas


def append_prepared_companion(output: list[str], bitmap: Image.Image, name: str, spec: dict) -> None:
    bitmap = load_pixel_animal(name) or bitmap
    width, height = bitmap.size
    bitmap.save(PREVIEW_DIR / f"{name}.png")
    prefix = name.upper()
    output.append(f"const uint8_t {prefix}_WIDTH = {width};")
    output.append(f"const uint8_t {prefix}_HEIGHT = {height};")
    output.append(format_array(
        f"{prefix}_BITMAP",
        encode_bitmap(bitmap, width, height),
    ))
    for pose_name, generated_pose in (
        ("HAPPY", happy_pose(bitmap, spec, name)),
        ("SLEEP", sleep_pose(bitmap, spec)),
    ):
        pose = load_pixel_animal(name, (width, height), pose_name.lower()) or generated_pose
        pose.save(PREVIEW_DIR / f"{name}-{pose_name.lower()}.png")
        output.append(format_array(
            f"{prefix}_{pose_name}_BITMAP",
            encode_bitmap(pose, width, height),
        ))


def append_companion(output: list[str], image: Image.Image, name: str, spec: dict) -> None:
    append_prepared_companion(output, prepare_bitmap(image, spec), name, spec)


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

    species_feed_sheet = Image.open(SPECIES_FEED_COMPANIONS_SOURCE)
    canonical_scene_companions = {
        "dog": prepare_companion_from_scene(
            prepare_grid_action_frames(species_feed_sheet, 0, 6)[0],
            SPECS["dog"]["width"],
            SPECS["dog"]["height"],
        ),
        "bunny": prepare_companion_from_scene(
            prepare_grid_action_frames(species_feed_sheet, 1, 6)[0],
            SPECS["bunny"]["width"],
            SPECS["bunny"]["height"],
        ),
    }
    for name, spec in SPECS.items():
        if name in canonical_scene_companions:
            append_prepared_companion(output, canonical_scene_companions[name], name, spec)
        else:
            append_companion(output, image, name, spec)

    for name, spec in NEW_COMPANION_SPECS.items():
        append_companion(output, new_companions, name, spec)

    for name, spec in FINAL_COMPANION_SPECS.items():
        append_companion(output, final_companions, name, spec)

    output.append(f"const uint8_t CAT_ACTION_SCENE_WIDTH = {ACTION_SCENE_WIDTH};")
    output.append(f"const uint8_t CAT_ACTION_SCENE_HEIGHT = {ACTION_SCENE_HEIGHT};")
    output.append(f"const uint8_t SPECIES_ACTION_SCENE_WIDTH = {ACTION_SCENE_WIDTH};")
    output.append(f"const uint8_t SPECIES_ACTION_SCENE_HEIGHT = {ACTION_SCENE_HEIGHT};")
    for action, scene_sheets in (
        ("feed", SPECIES_FEED_SHEETS),
        ("water", SPECIES_WATER_SHEETS),
        ("sleep", SPECIES_SLEEP_SHEETS),
    ):
        for source_path, animals in scene_sheets:
            sheet = Image.open(source_path)
            for row, animal in enumerate(animals):
                pixel_frames = load_pixel_action_frames(animal, action)
                prepared_frames = pixel_frames or prepare_grid_action_frames(sheet, row, len(animals))
                contact_sheet = Image.new("L", (ACTION_SCENE_WIDTH * 4, ACTION_SCENE_HEIGHT), 255)
                for frame_number, frame in enumerate(prepared_frames):
                    if pixel_frames is None:
                        frame = clean_species_scene_frame(animal, action, frame_number, frame)
                        frame = finish_action_frame(frame)
                    frame.save(PREVIEW_DIR / f"{animal}-{action}-scene-{frame_number}.png")
                    contact_sheet.paste(frame, (ACTION_SCENE_WIDTH * frame_number, 0))
                    runs = encode_rle(frame)
                    validate_rle(frame, runs)
                    output.append(format_array(
                        f"{animal.upper()}_{action.upper()}_{frame_number}_RLE",
                        runs,
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
        pixel_frames = load_pixel_action_frames("cat", action)
        frames = pixel_frames or prepare_action_frames(
            Image.open(ACTION_SCENE_DIR / f"{action}.png"),
            source_frames=source_frames,
        )
        contact_sheet = Image.new("L", (ACTION_SCENE_WIDTH * 4, ACTION_SCENE_HEIGHT), 255)
        for frame_number, frame in enumerate(frames):
            if pixel_frames is None:
                frame = clean_action_frame(action, frame_number, frame)
                frame = finish_action_frame(frame)
            frame.save(PREVIEW_DIR / f"cat-{action}-{frame_number}.png")
            contact_sheet.paste(frame, (ACTION_SCENE_WIDTH * frame_number, 0))
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
    egg = load_pixel_final_any_size(PIXEL_FINAL_DIR / "egg.png") or prepare_bitmap(egg_source, egg_spec)
    egg.save(PREVIEW_DIR / "egg.png")
    output.append(f"const uint8_t EGG_WIDTH = {egg.width};")
    output.append(f"const uint8_t EGG_HEIGHT = {egg.height};")
    output.append(format_array("EGG_BITMAP", encode_bitmap(egg, egg.width, egg.height)))

    OUTPUT.write_text("\n".join(output), encoding="ascii")


if __name__ == "__main__":
    main()
