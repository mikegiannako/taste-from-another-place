#!/bin/bash
set -e

PROJ="/mnt/c/Users/MikeG/Downloads/GameJamCSD2/GameJamCSD"

source /mnt/c/Users/MikeG/emsdk/emsdk_env.sh 2>/dev/null

echo "=== Building game for web ==="
cd "$PROJ"
make build-web 2>&1
echo "=== Build complete ==="
ls -lh "$PROJ/web/"
