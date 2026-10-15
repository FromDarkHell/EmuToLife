import argparse
import json
import os
import re
import sys
import urllib.request
from dataclasses import dataclass
from typing import Dict, List, Optional

import cv2
import numpy as np
import tqdm
from rembg import remove

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(SCRIPT_DIR, "data.txt")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
TAGS_FILE = os.path.join(
    SCRIPT_DIR, "..", "..", "..", "data", "json", "kamenrider", "tags.json"
)

ICON_SIZE = 100
WEBP_QUALITY = 70

# The "Char/ERC/Fig/Patch" column, eg "0x10/0x01/0x10/0x10<br/>(16/1/16/16)". The four bytes are
# packed big-endian into the tag's u32 id, eg 0x10/0x01/0x10/0x10 -> 0x10011010.
ID_BYTES_RE = re.compile(
    r"0x([0-9a-f]{2})/0x([0-9a-f]{2})/0x([0-9a-f]{2})/0x([0-9a-f]{2})", re.IGNORECASE
)
IMAGE_SRC_RE = re.compile(r'src="([^"]+)"')


@dataclass
class ChipRow:
    id: int
    character: str
    variant: str
    image_url: str


def parse_rows(markdown: str) -> List[ChipRow]:
    rows: List[ChipRow] = []

    for line in markdown.splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 5:
            continue

        id_match = ID_BYTES_RE.search(cells[3])
        image_match = IMAGE_SRC_RE.search(cells[4])
        if id_match is None or image_match is None:
            # Header/separator rows, or a malformed data row
            continue

        tag_id = int("".join(id_match.groups()), 16)
        rows.append(ChipRow(tag_id, cells[0], cells[1], image_match.group(1)))

    return rows


def load_known_tag_names() -> Dict[int, str]:
    """Flattens tags.json (including rebuilds) into an id -> name lookup."""
    with open(TAGS_FILE, "r", encoding="utf-8") as file:
        data = json.load(file)

    names: Dict[int, str] = {}
    for tags in data.values():
        for tag in tags:
            for build in [tag] + tag.get("rebuilds", []):
                names[build["id"]] = build["name"]

    return names


def download_image(url: str) -> Optional[np.ndarray]:
    """Fetches `url` and decodes it as a BGR(A) image."""
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(request, timeout=30) as response:
        buffer = np.frombuffer(response.read(), dtype=np.uint8)

    return cv2.imdecode(buffer, cv2.IMREAD_UNCHANGED)


def to_bgra(image: np.ndarray) -> np.ndarray:
    if image.ndim == 2:
        return cv2.cvtColor(image, cv2.COLOR_GRAY2BGRA)
    if image.shape[2] == 3:
        return cv2.cvtColor(image, cv2.COLOR_BGR2BGRA)
    return image


def has_transparency(image: np.ndarray) -> bool:
    return (
        image.ndim == 3 and image.shape[2] == 4 and bool(np.any(image[:, :, 3] < 255))
    )


def remove_background(image: np.ndarray) -> np.ndarray:
    """Runs rembg on a BGR(A) OpenCV image and returns a BGRA OpenCV image."""
    rgb_image = cv2.cvtColor(image[:, :, :3], cv2.COLOR_BGR2RGB)
    result_rgba: np.ndarray = remove(rgb_image)
    return cv2.cvtColor(result_rgba, cv2.COLOR_RGBA2BGRA)


def pad_to_square(image: np.ndarray) -> np.ndarray:
    """Centers a BGRA image on a transparent square canvas, so resizing keeps its aspect ratio."""
    height, width = image.shape[:2]
    size = max(height, width)

    canvas = np.zeros((size, size, 4), dtype=image.dtype)
    top = (size - height) // 2
    left = (size - width) // 2
    canvas[top : top + height, left : left + width] = image
    return canvas


def fix_images():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    with open(DATA_FILE, "r", encoding="utf-8") as file:
        rows = parse_rows(file.read())

    known_tag_names = load_known_tag_names()

    used_ids: Dict[int, ChipRow] = {}
    failed: List[str] = []

    for row in tqdm.tqdm(rows, desc="Converting chips"):
        label = f"{row.character} - {row.variant} (0x{row.id:08X} / {row.id})"

        if row.id in used_ids:
            original = used_ids[row.id]
            tqdm.tqdm.write(
                f"Skipping duplicate id for {label} // (orig: {original.character} - {original.variant})"
            )
            continue

        if row.id not in known_tag_names:
            tqdm.tqdm.write(f"Warning: {label} has no matching entry in tags.json")

        try:
            image = download_image(row.image_url)
        except Exception as error:
            tqdm.tqdm.write(f"Failed to download {label}: {error}")
            failed.append(label)
            continue

        if image is None:
            tqdm.tqdm.write(f"Failed to decode image for {label}")
            failed.append(label)
            continue

        image = to_bgra(image)
        if not has_transparency(image):
            image = remove_background(image)

        output_path = os.path.join(OUTPUT_DIR, f"{row.id}.webp")
        if not cv2.imwrite(
            output_path,
            cv2.resize(pad_to_square(image), (ICON_SIZE, ICON_SIZE)),
            [cv2.IMWRITE_WEBP_QUALITY, WEBP_QUALITY],
        ):
            tqdm.tqdm.write(f"Failed to write {output_path}")
            failed.append(label)
            continue

        used_ids[row.id] = row

    missing_images = sorted(set(known_tag_names) - set(used_ids))
    if missing_images:
        print(f"\n{len(missing_images)} tag(s) in tags.json have no image:", file=sys.stderr)
        for tag_id in missing_images:
            print(f"  {known_tag_names[tag_id]} ({tag_id})", file=sys.stderr)

    if failed:
        print(f"\n{len(failed)} image(s) could not be converted:", file=sys.stderr)
        for label in failed:
            print(f"  {label}", file=sys.stderr)

    print(f"\nConverted {len(used_ids)} image(s) to {OUTPUT_DIR}")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--images", action="store_true", default=False)

    args = parser.parse_args()

    if args.images:
        fix_images()


if __name__ == "__main__":
    main()
