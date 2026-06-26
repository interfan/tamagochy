from pathlib import Path

from PIL import Image, ImageEnhance, ImageOps


ROOT = Path(__file__).resolve().parents[1]
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"
PIXEL_FINAL_DIR = ROOT / "assets" / "pixel-final"
ANIMAL_DIR = PIXEL_FINAL_DIR / "animals"
ACTION_SCENE_DIR = PIXEL_FINAL_DIR / "action-scenes"
ICON_DIR = PIXEL_FINAL_DIR / "icons"

ANIMALS = ("cat", "dog", "bunny", "panda", "dragon", "fox", "chicken", "pig", "hamster", "penguin")
ANIMAL_POSES = ("", "happy", "sleep")
SPECIES_ACTIONS = ("feed", "water", "sleep", "medicine", "pet", "groom", "clean", "wash", "learn")
CAT_ACTIONS = ("feed", "water", "sleep", "overnight", "clean", "medicine", "learn", "pet", "groom", "wash")
ICONS = ("food", "water", "bed", "moon", "overnight", "clean", "wash", "learn", "pet", "medicine", "play", "groom")

ANIMAL_FRAME_SIZE = (120, 120)
ANIMAL_MAX_INK_SIZE = 108
ACTION_SCENE_SIZE = (184, 184)
ICON_SIZE = (28, 24)
EGG_FRAME_SIZE = (120, 130)
EGG_MAX_INK_SIZE = 116


def to_binary(image: Image.Image) -> Image.Image:
    image = ImageOps.autocontrast(image.convert("L"))
    image = ImageEnhance.Contrast(image).enhance(1.35)
    return image.point(lambda value: 0 if value < 180 else 255)


def fit_ink_to_frame(source: Image.Image, frame_size: tuple[int, int], max_ink_size: int) -> Image.Image:
    binary = to_binary(source)
    bbox = ImageOps.invert(binary).getbbox()
    if not bbox:
        return Image.new("L", frame_size, 255)

    cropped = binary.crop(bbox)
    scale = min(max_ink_size / cropped.width, max_ink_size / cropped.height)
    resized = cropped.resize(
        (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    resized = to_binary(resized)

    canvas = Image.new("L", frame_size, 255)
    x = (frame_size[0] - resized.width) // 2
    y = frame_size[1] - resized.height - 6
    canvas.paste(resized, (x, y))
    return canvas


def fit_ink_centered(source: Image.Image, frame_size: tuple[int, int], max_ink_size: int) -> Image.Image:
    binary = to_binary(source)
    bbox = ImageOps.invert(binary).getbbox()
    if not bbox:
        return Image.new("L", frame_size, 255)

    cropped = binary.crop(bbox)
    scale = min(max_ink_size / cropped.width, max_ink_size / cropped.height)
    resized = cropped.resize(
        (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    resized = to_binary(resized)

    canvas = Image.new("L", frame_size, 255)
    x = (frame_size[0] - resized.width) // 2
    y = (frame_size[1] - resized.height) // 2
    canvas.paste(resized, (x, y))
    return canvas


def copy_exact_binary(source: Path, destination: Path, size: tuple[int, int]) -> None:
    with Image.open(source) as image:
        if image.size != size:
            raise ValueError(f"{source} must be {size[0]}x{size[1]}, got {image.size[0]}x{image.size[1]}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        to_binary(image).save(destination)


def seed_animals() -> int:
    count = 0
    ANIMAL_DIR.mkdir(parents=True, exist_ok=True)
    for animal in ANIMALS:
        for pose in ANIMAL_POSES:
            suffix = f"-{pose}" if pose else ""
            source = PREVIEW_DIR / f"{animal}{suffix}.png"
            if not source.exists():
                continue
            with Image.open(source) as image:
                fit_ink_to_frame(image, ANIMAL_FRAME_SIZE, ANIMAL_MAX_INK_SIZE).save(
                    ANIMAL_DIR / f"{animal}{suffix}.png"
                )
            count += 1
    return count


def seed_action_scenes() -> int:
    count = 0
    ACTION_SCENE_DIR.mkdir(parents=True, exist_ok=True)
    for animal in ANIMALS:
        actions = CAT_ACTIONS if animal == "cat" else SPECIES_ACTIONS
        for action in actions:
            for frame in range(4):
                if animal == "cat":
                    source = PREVIEW_DIR / f"cat-{action}-{frame}.png"
                else:
                    source = PREVIEW_DIR / f"{animal}-{action}-scene-{frame}.png"
                if not source.exists():
                    continue
                copy_exact_binary(source, ACTION_SCENE_DIR / f"{animal}-{action}-{frame}.png", ACTION_SCENE_SIZE)
                count += 1
    return count


def seed_icons() -> int:
    count = 0
    ICON_DIR.mkdir(parents=True, exist_ok=True)
    for icon in ICONS:
        source = PREVIEW_DIR / f"action-icon-{icon}.png"
        if not source.exists():
            continue
        copy_exact_binary(source, ICON_DIR / f"{icon}.png", ICON_SIZE)
        count += 1
    return count


def seed_egg() -> int:
    source = PREVIEW_DIR / "egg.png"
    if not source.exists():
        return 0
    with Image.open(source) as image:
        fit_ink_centered(image, EGG_FRAME_SIZE, EGG_MAX_INK_SIZE).save(PIXEL_FINAL_DIR / "egg.png")
    return 1


def main() -> None:
    print(f"Seeded {seed_animals()} animal sprites")
    print(f"Seeded {seed_action_scenes()} action scene frames")
    print(f"Seeded {seed_icons()} action icons")
    print(f"Seeded {seed_egg()} egg sprite")


if __name__ == "__main__":
    main()
