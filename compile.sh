#!/usr/bin/env -S bash +x

set -o errexit
set -o nounset
set -o pipefail

mkdir -p build
(
  echo "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
  echo ${pkgdir}
  echo ${pkgdir-"/usr/local/bin"}
  cd build
  cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=${1-Release} -DCMAKE_INSTALL_PREFIX=/usr/local/bin -S ..
  make -j4 X11INC=/usr/include/X11 X11LIB=/usr/lib/X11 VERBOSE=1
)
