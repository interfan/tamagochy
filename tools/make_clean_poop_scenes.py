from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageOps


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

from make_companion_bitmaps import (  # noqa: E402
    clean_action_frame,
    connected_components,
    finish_action_frame,
    prepare_action_frames,
)


SOURCE_DIR = ROOT / "assets" / "action-scenes"
SCENE_DIR = ROOT / "assets" / "pixel-final" / "action-scenes"
PREVIEW_DIR = ROOT / "assets" / "bitmap-previews"

SPECIES_REFERENCE = SOURCE_DIR / "ai-clean-species-reference.png"
HAMSTER_REFERENCE = SOURCE_DIR / "ai-clean-hamster-reference.png"

SCENE_SIZE = 184
SOURCE_THRESHOLD = 145
FINAL_THRESHOLD = 150

REFERENCE_ROWS = ("dog", "bunny", "panda", "dragon", "fox", "chicken", "pig", "penguin")
ROW_CENTERS = (78, 199, 319, 439, 559, 681, 803, 925)
ROW_BOUNDS = (0,) + tuple(
    round((ROW_CENTERS[index] + ROW_CENTERS[index + 1]) / 2)
    for index in range(len(ROW_CENTERS) - 1)
) + (1024,)

MANUAL_CLEAR_RECTS = {
    ("pig", 2): ((45, 128, 84, 142),),
}


def binarize_source(image: Image.Image) -> Image.Image:
    gray = ImageOps.autocontrast(image.convert("L"))
    return gray.point(lambda value: 0 if value < SOURCE_THRESHOLD else 255)


def remove_tiny_noise(image: Image.Image) -> Image.Image:
    cleaned = image.copy()
    draw = ImageDraw.Draw(cleaned)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(image):
        width = max_x - min_x
        height = max_y - min_y
        if count <= 8 and width <= 7 and height <= 7:
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    return cleaned


def polish_frame(image: Image.Image, frame_number: int) -> Image.Image:
    cleaned = image.copy()
    draw = ImageDraw.Draw(cleaned)
    for min_x, min_y, max_x, max_y, center_x, count in connected_components(image):
        height = max_y - min_y
        center_y = (min_y + max_y) / 2
        top_stray = frame_number != 3 and center_y < 70 and count < 1000 and height < 45
        bottom_stray = center_y > 160 and count < 1200 and height < 35
        microscopic = count <= 3
        if top_stray or bottom_stray or microscopic:
            draw.rectangle((min_x, min_y, max_x - 1, max_y - 1), fill=255)
    return cleaned


def clear_manual_artifacts(image: Image.Image, animal: str, frame_number: int) -> Image.Image:
    cleaned = image.copy()
    draw = ImageDraw.Draw(cleaned)
    for rectangle in MANUAL_CLEAR_RECTS.get((animal, frame_number), ()):
        draw.rectangle(rectangle, fill=255)
    return cleaned


def fit_to_scene(
    image: Image.Image,
    frame_number: int,
    max_width: int = 170,
    max_height: int = 150,
) -> Image.Image:
    image = remove_tiny_noise(image)
    components = [component for component in connected_components(image) if component[5] >= 8]
    if not components:
        return Image.new("L", (SCENE_SIZE, SCENE_SIZE), 255)

    min_x = min(component[0] for component in components)
    min_y = min(component[1] for component in components)
    max_x = max(component[2] for component in components)
    max_y = max(component[3] for component in components)
    padding = 8
    min_x = max(0, min_x - padding)
    min_y = max(0, min_y - padding)
    max_x = min(image.width, max_x + padding)
    max_y = min(image.height, max_y + padding)

    cropped = image.crop((min_x, min_y, max_x, max_y))
    scale = min(max_width / cropped.width, max_height / cropped.height)
    resized = cropped.resize(
        (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    resized = resized.point(lambda value: 0 if value < FINAL_THRESHOLD else 255)

    canvas = Image.new("L", (SCENE_SIZE, SCENE_SIZE), 255)
    canvas.paste(
        resized,
        ((SCENE_SIZE - resized.width) // 2, (SCENE_SIZE - resized.height) // 2),
    )
    return polish_frame(remove_tiny_noise(canvas), frame_number)


def source_components(source: Image.Image) -> list[tuple[int, int, int, int, int, int]]:
    return [component for component in connected_components(source) if component[5] >= 8]


def extract_species_frame(
    source: Image.Image,
    components: list[tuple[int, int, int, int, int, int]],
    row: int,
    column: int,
) -> Image.Image:
    column_width = source.width // 4
    start_x = column * column_width
    end_x = (column + 1) * column_width
    start_y = ROW_BOUNDS[row]
    end_y = ROW_BOUNDS[row + 1]
    part = Image.new("L", (column_width, source.height), 255)

    for min_x, min_y, max_x, max_y, center_x, count in components:
        center_y = (min_y + max_y) / 2
        if start_x <= center_x < end_x and start_y <= center_y < end_y:
            part.paste(
                source.crop((min_x, min_y, max_x, max_y)),
                (min_x - start_x, min_y),
            )

    return fit_to_scene(part, column)


def extract_hamster_frame(
    source: Image.Image,
    components: list[tuple[int, int, int, int, int, int]],
    column: int,
) -> Image.Image:
    column_width = source.width // 4
    start_x = column * column_width
    end_x = (column + 1) * column_width
    part = Image.new("L", (column_width, source.height), 255)

    for min_x, min_y, max_x, max_y, center_x, count in components:
        if start_x <= center_x < end_x:
            part.paste(source.crop((min_x, min_y, max_x, max_y)), (min_x - start_x, min_y))

    return fit_to_scene(part, column, max_width=172, max_height=152)


def cat_clean_frames() -> list[Image.Image]:
    with Image.open(SOURCE_DIR / "clean.png") as sheet:
        frames = prepare_action_frames(sheet)
    return [
        finish_action_frame(clean_action_frame("clean", frame_number, frame))
        for frame_number, frame in enumerate(frames)
    ]


def generated_species_frames() -> dict[str, list[Image.Image]]:
    frames: dict[str, list[Image.Image]] = {}
    with Image.open(SPECIES_REFERENCE) as sheet:
        source = binarize_source(sheet)
        components = source_components(source)
        for row, animal in enumerate(REFERENCE_ROWS):
            frames[animal] = [
                clear_manual_artifacts(
                    extract_species_frame(source, components, row, column),
                    animal,
                    column,
                )
                for column in range(4)
            ]

    with Image.open(HAMSTER_REFERENCE) as sheet:
        source = binarize_source(sheet)
        components = source_components(source)
        frames["hamster"] = [
            extract_hamster_frame(source, components, column)
            for column in range(4)
        ]

    return frames


def save_frames(animal: str, frames: list[Image.Image]) -> None:
    sheet = Image.new("L", (SCENE_SIZE * 4, SCENE_SIZE), 255)
    for frame_number, frame in enumerate(frames):
        frame = frame.point(lambda value: 0 if value < 128 else 255)
        frame.save(SCENE_DIR / f"{animal}-clean-{frame_number}.png")
        sheet.paste(frame, (frame_number * SCENE_SIZE, 0))
        if animal == "cat":
            frame.save(PREVIEW_DIR / f"cat-clean-{frame_number}.png")
        else:
            frame.save(PREVIEW_DIR / f"{animal}-clean-scene-{frame_number}.png")

    if animal == "cat":
        sheet.save(PREVIEW_DIR / "cat-clean-sheet.png")
    else:
        sheet.save(PREVIEW_DIR / f"{animal}-clean-scene-sheet.png")


def main() -> None:
    SCENE_DIR.mkdir(parents=True, exist_ok=True)
    PREVIEW_DIR.mkdir(parents=True, exist_ok=True)

    save_frames("cat", cat_clean_frames())
    for animal, frames in generated_species_frames().items():
        save_frames(animal, frames)

    print("Redrew poop-cleaning animations from clean generated source art")


if __name__ == "__main__":
    main()
