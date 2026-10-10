#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 /path/to/directory"
    exit 1
fi

TARGET_DIR="$1"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory $TARGET_DIR does not exist."
    exit 1
fi

echo "Running conditional height checks and cropping in $TARGET_DIR..."
echo "--------------------------------------------------------"

find "$TARGET_DIR" -type f \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.tiff" \) -print0 | while IFS= read -r -d '' file; do
    
    # Ignore existing backup files
    if [[ "$file" == *.bak ]]; then
        continue
    fi

    # Get the image height using ImageMagick identify
    height=$(magick identify -format "%h" "$file" 2>/dev/null)

    # Safety check if identify failed to read the file
    if [ -z "$height" ]; then
        echo "[ERROR] Could not read dimensions for: $file"
        continue
    fi

    # Condition 1: Exactly 48px tall
    if [ "$height" -eq 48 ]; then
        echo "[SKIP] Already 48px: $file"
        continue

    # Condition 2: Exactly 72px tall (48px base + 24px extra)
    elif [ "$height" -eq 72 ]; then
        if [ -f "$file.bak" ]; then
            echo "[SKIP] 72px but backup already exists: $file"
            continue
        fi
        
        echo "[CROP] 72px -> Cropping 24px from: $file"
        cp "$file" "$file.bak"
        magick "$file" -crop +0+24 "$file"

    # Condition 3: Greater than 72px
    elif [ "$height" -gt 72 ]; then
        echo "[NOTE] Large Image (Height: ${height}px): $file"
        continue
        
    # Catch-all for anything else (e.g., images smaller than 48px or between 49-71px)
    else
        echo "[SKIP] Untouched (Height: ${height}px): $file"
    fi

done

echo "--------------------------------------------------------"
echo "Process complete!"
