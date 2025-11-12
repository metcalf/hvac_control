#!/bin/bash

# Get today's date in YYYYMMDD format
DATE=$(date +%Y%m%d)

# Create the destination directory
DEST_DIR="logs/${DATE}"
mkdir -p "$DEST_DIR"

# Copy files from remote server using rsync with sudo
echo "Copying logs to ${DEST_DIR}..."
rsync -av --rsync-path="sudo rsync" andrew@home-sbc-server-local.itsshedtime.com:/var/log/remote/ "$DEST_DIR/"

echo "Done! Logs saved to ${DEST_DIR}"
