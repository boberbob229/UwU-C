#!/bin/bash
set -e

IMAGE=uwuos
ROOT="$(cd "$(dirname "$0")" && pwd)"
KERNEL="$ROOT/kernel"
BUILD="$KERNEL/build"
ISO_DIR="$BUILD/iso"

echo "building docker image"
docker build --platform=linux/amd64 -t "$IMAGE" "$ROOT"

echo "building kernel"
docker run --rm \
  --platform=linux/amd64 \
  -v "$ROOT:/kernel" \
  "$IMAGE" \
  bash -c "cd /kernel/kernel && make clean && make"

echo
echo "1) img"
echo "2) iso"
echo "3) both"
read -p "> " choice

if [[ "$choice" == "1" ]]; then
  echo "done -> $BUILD/uwuos.img"
  exit 0
fi

echo "making iso"

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR"


cp "$BUILD/uwuos.img" "$ISO_DIR/boot.img"

docker run --rm \
  --platform=linux/amd64 \
  -v "$ROOT:/kernel" \
  "$IMAGE" \
  bash -c "
    xorriso -as mkisofs \
      -R -J \
      -b boot.img \
      -no-emul-boot \
      -boot-load-size 4 \
      -boot-info-table \
      -o /kernel/kernel/build/uwuos.iso \
      /kernel/kernel/build/iso
  "

echo "iso -> $BUILD/uwuos.iso"

if [[ "$choice" == "3" ]]; then
  echo "img -> $BUILD/uwuos.img"
fi
