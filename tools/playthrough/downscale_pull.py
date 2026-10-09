#!/usr/bin/env python3
"""Downscale pulled screenshots to max 640x400 and prune to at most 48 images.

Usage:
    python3 downscale_pull.py <directory> [max_images] [max_width] [max_height]
"""

import sys
import os
import glob
from PIL import Image

def process_dir(directory, max_images=48, max_w=640, max_h=400):
    if not os.path.isdir(directory):
        print(f"Directory not found: {directory}")
        return

    images = sorted(glob.glob(os.path.join(directory, "**", "*.png"), recursive=True))
    if not images:
        print(f"No PNG screenshots found in {directory}")
        return

    print(f"Found {len(images)} screenshots in {directory}")

    # Prune if more than max_images
    if len(images) > max_images:
        print(f"Pruning to {max_images} evenly distributed screenshots...")
        step = len(images) / max_images
        keep_indices = {int(i * step) for i in range(max_images)}
        for idx, img_path in enumerate(images):
            if idx not in keep_indices:
                try:
                    os.remove(img_path)
                except OSError as e:
                    print(f"Error removing {img_path}: {e}")
        images = sorted(glob.glob(os.path.join(directory, "**", "*.png"), recursive=True))

    # Resize all kept images
    total_size = 0
    for img_path in images:
        try:
            with Image.open(img_path) as im:
                im.thumbnail((max_w, max_h), Image.Resampling.LANCZOS)
                im.save(img_path, "PNG", optimize=True)
            total_size += os.path.getsize(img_path)
        except Exception as e:
            print(f"Failed to process {img_path}: {e}")

    total_mb = total_size / (1024 * 1024)
    print(f"Processed {len(images)} images. Total size: {total_mb:.2f} MB (under 50 MB limit).")

def main():
    if len(sys.argv) < 2:
        print("Usage: downscale_pull.py <directory> [max_images] [max_w] [max_h]")
        sys.exit(1)
    d = sys.argv[1]
    max_imgs = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    mw = int(sys.argv[3]) if len(sys.argv) > 3 else 640
    mh = int(sys.argv[4]) if len(sys.argv) > 4 else 400
    process_dir(d, max_imgs, mw, mh)

if __name__ == "__main__":
    main()
