# Arch Linux (https://www.archlinux.org) packaging script.
# Run: `/usr/bin/makepkg --sign` in the source directory to build signed Arch Linux
# package
pkgname=cdns
pkgver=1.0.0
pkgrel=1
pkgdesc="Cryptographic DNS resolver"
arch=('x86_64')
url="https://github.com/mmstv/$pkgname"
license=('GPL')
replaces=('dnscrypt-proxy' 'dnscrypt-wrapper')
depends=('libsodium>=1.0.10' 'libev>=4.24' 'systemd>=230')
makedepends=('ninja>=1.6' 'cmake>=3.9')
install='pkg.install'
backup=("etc/$pkgname/$pkgname.conf")

build() {
	mkdir -p _build
	cd _build
	# cmake picks up CFLAGS, CXXFLAGS, and LDFLAGS but not CPPFLAGS from the
	# environment
	CXXFLAGS+=" $CPPFLAGS"
	# default CMake GNU libdir is lib64
	cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_SBINDIR=bin -DCMAKE_INSTALL_LIBDIR=lib \
		-DCMAKE_INSTALL_SYSCONFDIR=/etc \
		-DBUILD_TESTING=OFF \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
		${startdir}
	ninja
}

package() {
	export DESTDIR="$pkgdir"
	cd _build
	ninja install
}
