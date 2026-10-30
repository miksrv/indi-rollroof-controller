#!/bin/bash
# Build and install the indi_rollroof driver (binary to /usr/bin, XML to /usr/share/indi)
set -e

cd "$(dirname "$0")"
rm -rf build
mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ../indi-rollroof/
make -j"$(nproc)"
sudo make install
