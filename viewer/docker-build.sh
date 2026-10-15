#!/bin/sh
# Build sdviewer for Debian 12 inside a debian:bookworm container.
# Usage (from the repo root, on any host with Docker):
#   docker run --rm -v "$PWD":/src debian:bookworm sh /src/viewer/docker-build.sh
# Output: dist/sdviewer (TurboJPEG linked statically; needs SDL2, FFmpeg and libva at runtime).
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential cmake pkg-config \
    libsdl2-dev libavcodec-dev libavutil-dev libva-dev libturbojpeg0-dev >/dev/null

cmake -S /src/viewer -B /tmp/build -DCMAKE_BUILD_TYPE=Release -DSDV_STATIC_TURBOJPEG=ON
cmake --build /tmp/build -j"$(nproc)"
mkdir -p /src/dist
cp /tmp/build/sdviewer /src/dist/sdviewer
echo "built: dist/sdviewer"
