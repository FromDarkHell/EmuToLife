import argparse
import glob
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional

import cv2
import numpy as np
import textdistance
import tqdm
from rembg import remove

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "data")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")

characters: List[Dict[str, Any]] = json.load(
    open(os.path.join(DATA_DIR, "characters.json"), "r")
)
vehicles: List[Dict[str, Any]] = json.load(
    open(os.path.join(DATA_DIR, "vehicles.json"), "r")
)
ALL_TAGS: List[Dict[str, Any]] = characters + vehicles

# Cover images live under "coins/<Category>/covers/...", and each category only ever contains
# one kind of tag - restricting matches to it up front rules out cross-category collisions, eg
# a "Play Set" cover matching the "Spider-Man" figure just because its name contains that word.
CATEGORY_TAG_TYPES: Dict[str, set] = {
    "Characters": {"figure"},
    "Play_Sets": {"playset"},
    "Toy_Box_Game": {"playset"},
    "Power_Discs": {"power_disc"},
}

# Trailing type/subtype annotations, eg "- Toy (Vehicle)", "- Ability", "- Customization (Terrain)"
TRAILING_TYPE_RE = re.compile(
    r"\s*-\s*(Toy|Ability|Customization)\s*(\([^)]*\))?\s*$", re.IGNORECASE
)
# Leading article on the franchise/item segment, eg "The Lone Ranger" - some tag names in the
# JSON data drop it (eg "Lone Ranger") while others keep it as part of the name (eg "The Rip"),
# so both the with- and without-article forms of a candidate are tried against every tag.
LEADING_ARTICLE_RE = re.compile(r"^The\s+", re.IGNORECASE)

# "Ability (Team Up)" cover files correspond to tags named "<Something> Team-Up: <Name>", eg
# "Marvel Team-Up: Winter Soldier" or "Galactic Team-Up: Mace Windu" - strip that prefix off the
# tag name so it can be compared against the bare item name from the filename.
TEAM_UP_PREFIX_RE = re.compile(r"^.*\bTeam-?Up:\s*", re.IGNORECASE)

MATCH_THRESHOLD = 0.85
# jaro_winkler's prefix bonus over-rewards names that merely share a leading word (eg "Mickey's
# Car" scoring 0.90 against "Mickey's Sorcerer Hat"); require overall substring overlap too.
RATCLIFF_GATE = 0.7


def candidate_name_from_filename(stem: str) -> str:
    """Strip the leading franchise segment and trailing type annotation from a cover filename.

    Parenthetical variant annotations, eg "(Crystal)", "(Glowing)", "(Light FX)", are kept -
    those variants have their own distinct tag ids and must stay matchable as such.
    """
    name = TRAILING_TYPE_RE.sub("", stem).strip()

    segments = [segment.strip() for segment in name.split(" - ") if segment.strip()]
    if len(segments) > 1:
        # Every segment but the last is franchise/park labeling (eg "Disney Parks", "Marvel
        # Comics"); only the last segment names the toy itself.
        segments = segments[-1:]

    return " - ".join(segments)


def name_similarity(a: str, b: str) -> float:
    a, b = a.lower().strip(), b.lower().strip()
    if not a or not b:
        return 0.0

    if a == b:
        return 1.0

    shorter, longer = (a, b) if len(a) <= len(b) else (b, a)
    if re.search(r"\b" + re.escape(shorter) + r"\b", longer):
        # A whole-word containment match is a strong signal, but must stay weaker than an exact
        # match so eg "Lightning McQueen" doesn't outscore an exact "Lightning McQueen (Crystal)".
        # Scaled by how much of the longer string it covers, so eg "Hulkbuster" (most of
        # "Hulkbuster Iron Man") outscores the co-contained "Iron Man" (less of it).
        return 0.9 + 0.05 * (len(shorter) / len(longer))

    if textdistance.ratcliff_obershelp.normalized_similarity(a, b) < RATCLIFF_GATE:
        return 0.0

    return textdistance.jaro_winkler.normalized_similarity(a, b)


def category_from_image_path(image_path: str) -> str:
    # .../coins/<Category>/covers/<file>.png
    covers_dir = os.path.dirname(image_path)
    category_dir = os.path.dirname(covers_dir)
    return os.path.basename(category_dir)


def find_best_match(
    candidate_name: str, tags: List[Dict[str, Any]]
) -> Optional[Dict[str, Any]]:
    candidate_variants = {candidate_name, LEADING_ARTICLE_RE.sub("", candidate_name)}

    best_tag: Optional[Dict[str, Any]] = None
    best_score = 0.0

    for tag in tags:
        tag_name_variants = {tag["name"], TEAM_UP_PREFIX_RE.sub("", tag["name"])}

        for candidate_variant in candidate_variants:
            for tag_name_variant in tag_name_variants:
                score = name_similarity(candidate_variant, tag_name_variant)
                if score > best_score:
                    best_score = score
                    best_tag = tag

    if best_tag is None or best_score < MATCH_THRESHOLD:
        return None

    return best_tag


def has_transparency(image: np.ndarray) -> bool:
    return (
        image.ndim == 3 and image.shape[2] == 4 and bool(np.any(image[:, :, 3] < 255))
    )


def remove_background(image: np.ndarray) -> np.ndarray:
    """Runs rembg on a BGR(A) OpenCV image and returns a BGRA OpenCV image."""
    rgb_image = cv2.cvtColor(
        image[:, :, :3] if image.ndim == 3 and image.shape[2] == 4 else image,
        cv2.COLOR_BGR2RGB,
    )
    result_rgba: np.ndarray = remove(rgb_image)
    return cv2.cvtColor(result_rgba, cv2.COLOR_RGBA2BGRA)


def fix_images():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    image_paths = sorted(
        glob.glob(os.path.join(DATA_DIR, "**", "covers", "*.png"), recursive=True)
    )

    used_ids: Dict[int, str] = {}
    unmatched: List[str] = []

    for image_path in tqdm.tqdm(image_paths, desc="Converting covers"):
        category = category_from_image_path(image_path)
        allowed_tag_types = CATEGORY_TAG_TYPES.get(category)
        if allowed_tag_types is None:
            tqdm.tqdm.write(f"Unknown category {category!r}, skipping: {image_path}")
            unmatched.append(image_path)
            continue

        candidate_tags = [
            tag for tag in ALL_TAGS if tag["tagType"] in allowed_tag_types
        ]

        stem = os.path.splitext(os.path.basename(image_path))[0]
        candidate_name = candidate_name_from_filename(stem)
        tag = find_best_match(candidate_name, candidate_tags)

        if tag is None:
            unmatched.append(image_path)
            continue

        tag_id = tag["id"]
        if tag_id in used_ids:
            tqdm.tqdm.write(
                f"Skipping duplicate for id {tag_id} ({tag['name']}) // (orig: {used_ids[tag_id]!r} // new: {image_path!r})"
            )
            continue

        image = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
        if image is None:
            tqdm.tqdm.write(f"Failed to read image: {image_path}")
            continue

        if not has_transparency(image):
            image = remove_background(image)

        output_path = os.path.join(OUTPUT_DIR, f"{tag_id}.webp")

        ICON_SIZE = 100

        success = cv2.imwrite(
            output_path,
            cv2.resize(image, (ICON_SIZE, ICON_SIZE)),
            [cv2.IMWRITE_WEBP_QUALITY, 70],
        )

        used_ids[tag_id] = image_path

    if unmatched:
        print(
            f"\n{len(unmatched)} image(s) could not be matched to a known tag:",
            file=sys.stderr,
        )
        for image_path in unmatched:
            print(f"  {image_path}", file=sys.stderr)

    print(f"\nConverted {len(used_ids)} image(s) to {OUTPUT_DIR}")


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--images", action="store_true", default=False)

    args = parser.parse_args()

    if args.images:
        fix_images()


if __name__ == "__main__":
    main()
