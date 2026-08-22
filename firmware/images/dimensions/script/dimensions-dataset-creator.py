import argparse
import json
import glob
import os
import sys
from typing import Any, Dict, List, Tuple
import cv2
import numpy as np
import easyocr
import textdistance
import tqdm

characters = json.load(open("./data/characters.json", "r"))
vehicles = json.load(open("./data/vehicles.json", "r"))

IDENTIFIED_TAGS: Dict[int, Tuple[Dict[str, Any], np.ndarray]] = {}


def extract_circles_from_template(
    img, min_radius: int = 100, max_radius: int = 200, size: int = 312
) -> List[np.ndarray]:
    """
    Extract circular regions from a template image containing multiple circular elements.

    Parameters:
    - image_path: Path to the input image
    - output_dir: Directory to save extracted circles
    - min_radius: Minimum radius for circle detection
    - max_radius: Maximum radius for circle detection
    """

    # Create a copy for drawing
    output_img = img.copy()

    # Convert to grayscale for circle detection
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # Apply Gaussian blur to reduce noise
    blurred = cv2.GaussianBlur(gray, (9, 9), 2)

    # Use HoughCircles to detect circles
    circles = cv2.HoughCircles(
        blurred,
        cv2.HOUGH_GRADIENT,
        dp=1,
        minDist=300,  # Minimum distance between circle centers
        param1=50,  # Upper threshold for edge detection
        param2=30,  # Accumulator threshold for center detection
        minRadius=min_radius,
        maxRadius=max_radius,
    )

    results = []

    if circles is not None:
        circles = np.round(circles[0, :]).astype("int")
        print(f"Found {len(circles)} circles")

        # Sort circles by position (top to bottom, left to right)
        circles = sorted(circles, key=lambda x: (x[1], x[0]))

        for i, (x, y, r) in enumerate(circles):
            # Create a mask for the circular region
            mask = np.zeros(gray.shape, dtype=np.uint8)
            cv2.circle(mask, (x, y), r, 255, -1)

            # Extract the circular region with padding
            padding = 10
            x1 = max(0, x - r - padding)
            y1 = max(0, y - r - padding)
            x2 = min(img.shape[1], x + r + padding)
            y2 = min(img.shape[0], y + r + padding)

            # Crop the region
            cropped_img = img[y1:y2, x1:x2]
            cropped_mask = mask[y1:y2, x1:x2]

            # Create a circular crop with transparent background
            # First, create an image with alpha channel
            h, w = cropped_img.shape[:2]
            result = np.zeros((h, w, 4), dtype=np.uint8)
            result[:, :, :3] = cropped_img
            result[:, :, 3] = cropped_mask  # Alpha channel

            result = cv2.resize(result, (size, size))

            cv2.circle(output_img, (x, y), r, (0, 255, 0), 2)
            cv2.putText(
                output_img,
                str(i + 1),
                (x - 10, y + 5),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (255, 0, 0),
                2,
            )

            results.append(result)

    return (results, output_img)


def fix_images():
    ocr = easyocr.Reader(["en"])

    for idx, image_path in tqdm.tqdm(
        list(enumerate(sorted(glob.glob("./data/templates/*.png"))))
    ):
        image = cv2.imread(image_path)

        tags, labelled = extract_circles_from_template(image)
        for tag_idx, tag in enumerate(tags):
            ocr_matches = ocr.readtext(
                cv2.cvtColor(tag, cv2.COLOR_BGRA2BGR),
                decoder="beamsearch",
                min_size=50,
                paragraph=True,
                contrast_ths=0.0,
                height_ths=2.0,
                y_ths=0.7,
                x_ths=3.0,
            )

            assert ocr_matches

            text = ocr_matches[0][1]

            replacements = {
                # Characters
                "McFly Marty": "Marty McFly",
                "Jates Abby": "Abby Jates",
                "Dr Who": "The Doctor",
                "Lumpy Princess": "Lumpy Space Princess",
                "Acu": "ACU Trooper",
                "Krusty": "Krusty the Clown",
                "Homer": "Homer Simpson",
                "Marceline": "Marceline the Vampire Queen",
                "Wicked Witch": "Wicked Witch of the West",
                "Finn": "Finn the Human",
                "Jake": "Jake the Dog",
                "Sonic": "Sonic the Hedgehog",
                "BA.": "B.A. Baracus",
                # Vehicles
                "Electro- Shooter": "Electro-Shooter",
                "Sword 'Projector Dragon": "Sword Projector Dragon",
                "Blast Bot Mega": "Mega Blast Bot",
                "Terror Destroyer Dog": "Terror Dog Destroyer",
                "Shooter": "Laser Shooter",
                "Laserbot": "Laser Robot Walker",
                "Phone Home W0o goe": "Phone Home",
                "Super ICharged Satellitel": "Super-Charged Satellite",
                "Destruct 0 Mech": "Destruct-o-Mech",
                "Tandem War Elephant": "Ancient Psychic Tandem War Elephant",
                "Laser Pulse TARDIS": "Laser-Pulse TARDIS",
                "Energy Burst TARDIS": "Energy-Burst TARDIS",
                "Ecto-]": "Ecto-1",
                "Ecto-] Water Diver": "Ecto-1 Water Diver",
                "Ecto-] Blaster": "Ecto-1 Blaster",
                "Sport Car": "IMF Sports Car",
                "Scrambler": "IMF Scrambler",
                "Pirate Ship": "One-Eyed Willy's Pirate Ship",
                "Skele- Turkey": "Skele-Turkey",
                "Homercraft": "The Homercraft",
                "SubmaHomer": "The SubmaHomer",
                "Taunt 0 Vision": "Taunt-o-Vision",
                "MechaHomer": "The MechaHomer",
                "BatBlaster": "Batblaster",
                "Lord Vortech": "Lord Vortech (unreleased)",
                "The PerfEcto": "PerfEcto",
                "Black Thunder": "The Black Thunder",
                "Joker' s Chopper": "The Joker's Chopper",
                "Misile Launcher": "Missile Launcher",
                "Lock n Laser Jet": "Lock 'n' Laser Jet",
                "Gadget 0 Matic": "Gadget-O-Matic",
                "Stripe s Throne": "Stripe's Throne",
                "Bomber Blaster": "Boulder Blaster",
                "PPH Hotline": "PPG Hotline",
                "PPG Mag-Net": "Powerpuff Mag-Net",
                "Mystery Tow&Go": "Mystery Tow & Go",
                "Forklift": "T-Forklift",
            }

            # Perform replacement if exact match exists
            text = replacements.get(text, text)

            # Handle special substring replacement
            if "KITT" in text:
                text = text.replace("KITT", "K.I.T.T.")

            # These are some more misc fixes, which need some extra logic added into them.
            DUPLICATE_NAMES = {
                # Aqua Watercraft
                ("Aqua Watercraft", 1): "Seven Seas Speeder",
                ("Aqua Watercraft", 2): "Trident of Fire",
                # Drill Driver
                ("Drill Driver", 7): "Bane Dig 'n' Drill",
                ("Drill Driver", 8): "Bane Drill 'n' Blast",
                # Ghostbusters (2016)
                ("Ecto-1", 22): "Ecto-1 (2016)",
            }

            for tu, val in DUPLICATE_NAMES.items():
                if tu[0] == text and tag_idx == tu[1]:
                    print(f"Force-fixing {text=} {tag_idx=} should be {val}")
                    text = val
                    break

            match = None
            for character in characters + vehicles:

                # Some characters have "similar" names in terms of distance
                if (
                    (text == "Bane" and character["name"] == "Bane")
                    or (text == "Jay" and character["name"] == "Jay")
                    or (text == "Kai" and character["name"] == "Kai")
                ):
                    match = character
                    break

                if textdistance.levenshtein.distance(text, character["name"]) <= 2:
                    match = character

            assert match

            if match["id"] in IDENTIFIED_TAGS:
                print(f"Found duplicate {text=} {tag_idx=} {match=}")

            IDENTIFIED_TAGS[match["id"]] = (match, tag)

            file_path = os.path.join(
                "./output",
                "characters" if match in characters else "vehicles",
                f"{str(match['id']).zfill(4)}.webp",
            )

            ICON_SIZE = 100

            success = cv2.imwrite(
                file_path,
                cv2.resize(tag, (ICON_SIZE, ICON_SIZE)),
                [cv2.IMWRITE_WEBP_QUALITY, 70],
            )

            assert success

    for c in characters + vehicles:
        if c["id"] not in IDENTIFIED_TAGS.keys():
            print(f"Failed to find {c=} inside tags")


def fix_json():
    fixed_characters = []

    for c in characters:
        c["abilities"] = c["abilities"].split(",")
        fixed_characters.append(c)
    json.dump(fixed_characters, open("./output/characters.json", "w"))

    # This fixes *all* vehicles abilities, even rebuilds
    for v in vehicles:
        v["abilities"] = v["abilities"].split(",")

    organized_vehicles = []
    all_rebuilds = []
    for v in vehicles:
        if v in all_rebuilds:
            continue

        if v["rebuild"] == 0:
            rebuilds = []

            for v2 in vehicles:
                if v2["rebuild"] != 0 and v2["id"] - v2["rebuild"] == v["id"]:
                    v2["name"] = v2["name"].strip(" *")
                    rebuilds.append(v2)

            print(f"Found {len(rebuilds)} rebuilds for {v['id']}")
            v["name"] = v["name"].strip(" *")
            v["rebuilds"] = rebuilds
            all_rebuilds.extend(rebuilds)

            organized_vehicles.append(v)

    json.dump(organized_vehicles, open("./output/vehicles.json", "w"))


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--images", action="store_true", default=False)
    parser.add_argument("--json", action="store_true", default=False)

    args = parser.parse_args()

    if args.images:
        fix_images()

    if args.json:
        fix_json()


if __name__ == "__main__":
    main()
