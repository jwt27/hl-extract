#!/bin/bash
set -e
make -j4
./hl-extract
cd extracted/
../hl-convert-snd
../hl-convert-ggs
