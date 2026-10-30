#!/bin/bash
# Build and install the indi_rollroof driver (binary to /usr/bin, XML to /usr/share/indi)
set -e

cd "$(dirname "$0")"
rm -rf build
mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ../indi-rollroof/
make -j"$(nproc)"

# root (e.g. in a container) installs directly, a regular user through sudo
if [ "$(id -u)" -eq 0 ]; then
    make install
else
    sudo make install
fi
