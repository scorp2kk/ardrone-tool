#!/bin/bash

set -e
set -x

mkdir a
mkdir b
mkdir fs

./plftool -d -e raw -i ../../../airborne-cargo-drone/fw/AirborneCargo.plf -o a/
./plftool -d -e raw -i a/001_0x03_0_main_boot.plf -o b/
python3 recover_file.py

