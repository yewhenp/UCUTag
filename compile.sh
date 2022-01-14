#!/usr/bin/env -S bash +x

set -o errexit
set -o nounset
set -o pipefail

mkdir -p build
(
  cd build
  cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=${1-Release} -S ..
  make -j4 X11INC=/usr/include/X11 X11LIB=/usr/lib/X11
)
