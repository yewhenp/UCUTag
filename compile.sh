#!/bin/bash +x

set -o errexit
set -o nounset
set -o pipefail

mkdir -p build
(
  cd build
  cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=${1-Release} ..
  make
)
#rm -rf build
