#!/bin/bash

docker run --rm -it -v "$(pwd):/kernel" uwuos qemu-system-x86_64 -fda build/uwuos.img -boot a -nographic
