FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-multilib \
    g++-multilib \
    nasm \
    qemu-system-x86 \
    xorriso \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /kernel

CMD ["/bin/bash"]
