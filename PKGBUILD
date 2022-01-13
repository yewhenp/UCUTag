# This is an example PKGBUILD file. Use this as a start to creating your own,
# and remove these comments. For more information, see 'man PKGBUILD'.
# NOTE: Please fill out the license field for your package! If it is unknown,
# then please put 'unknown'.

# Maintainer: Pavlo Hilei <p.hilei@ucu.edu.ua>
pkgname=ucutag
pkgver=1.0
pkgrel=1
epoch=
pkgdesc="This is fuse based tag-oriented filesystem that uses MongoDB as backend"
arch=(x86_64)
url="https://github.com/yewhenp/UCUTag.git"
license=('GPL3')
groups=()
depends=('boost-libs>=1.75' fuse2 mongo-c-driver mongo-cxx-driver mongodb-bin  )
makedepends=(git 'cmake>=3.15''boost>=1.75')
checkdepends=()
optdepends=()
provides=(ucutag)
conflicts=()
replaces=()
backup=()
options=()
install=
changelog=
source=("git+$url")
noextract=()
md5sums=('SKIP')
validpgpkeys=()

prepare() {
	cd UCUTag
	printf "1.0.r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
	cd UCUTag
	bash compile.sh
}

package() {
	cd UCUTag
	mkdir -p 
}
