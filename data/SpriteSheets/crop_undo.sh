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

echo "Restoring original images from .bak files in $TARGET_DIR..."

find "$TARGET_DIR" -type f -name "*.bak" -print0 | while IFS= read -r -d '' backup_file; do
    # Determine the original file name by stripping '.bak' from the end
    original_file="${backup_file%.bak}"
    
    echo "Restoring: $original_file"
    
    # Move the backup back over the cropped file
    mv "$backup_file" "$original_file"
done

echo "Restore complete!"
