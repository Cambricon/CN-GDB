#!/bin/bash
set -e

WORKSPACE="${PWD}"
BUILDDIR="$WORKSPACE/build/objdir"
INSTALLDIR="$WORKSPACE/build/neuware"
LIBPATH="$WORKSPACE/lib"
PATH="/usr/lib/ccache:${PATH}"
echo $PWD
echo $WORKSPACE
DEBUG_MODE=0
CFLAGS="$CFLAGS -O2"
CXXFLAGS="$CXXFLAGS -O2"

BUILD_TYPE="internal"
BUILD_TYPE_IDENTIFIER=""
echo Build Mode: $BUILD_TYPE

BUILD_MODE="drv-1m"
LIBS="$PWD/lib/libcngdb_backend.so $LIBS"
LD_LIBRARY_PATH="$PWD/lib:$LD_LIBRARY_PATH"
LDFLAGS="-Wl,-rpath=$DRV1MBUILD:$LDFLAGS"

echo "-- Build in mode: ${BUILD_MODE}"

rm -rf ${BUILDDIR}
mkdir -p ${BUILDDIR}
pushd ${BUILDDIR}

export CFLAGS="$CFLAGS -fstack-protector-strong -Wformat -Werror=format-security -std=gnu99 -pthread"
export CXXFLAGS="$CXXFLAGS -fstack-protector-strong -Wformat -Werror=format-security -std=c++11"
export LDFLAGS="$LDFLAGS -Wl,-Bsymbolic-functions -Wl,-z,relro"
export LIBS=$LIBS
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH

${WORKSPACE}/configure \
  --build=x86_64-linux-gnu \
  --prefix=/usr/local/neuware \
  --includedir="\${prefix}/include" \
  --mandir="\${prefix}/share/cngdb/man" \
  --infodir="\${prefix}/share/cngdb/info" \
  --datarootdir="\${prefix}/share/cngdb" \
  --libdir="\${prefix}/lib64" \
  --sysconfdir=/etc \
  --localstatedir=/var \
  --libexecdir="\${prefix}/lib/gdb"  \
  --disable-maintainer-mode \
  --disable-dependency-tracking \
  --disable-silent-rules   \
  --host=x86_64-linux-gnu \
  --disable-gdbtk \
  --disable-shared \
  --with-pkgversion="CNGDB v1.2.0 ${BUILD_TYPE_IDENTIFIER}" \
  --srcdir=${WORKSPACE} \
  --disable-readline \
  --with-system-readline \
  --with-expat \
  --with-system-zlib \
  --without-lzma \
  --without-guile \
  --without-babeltrace \
  --with-system-gdbinit=/etc/gdb/gdbinit \
  --enable-tui \
  --with-lzma \
  --enable-targets="x86_64-unknown-linux-gnu,m68k-unknown-linux-gnu" \
  --cngdb-${BUILD_MODE} \
  --cngdb-log-fatal \
  --cngdb-type-${BUILD_TYPE}
make -j64

echo Stripping gdb and libcngdb_backend.so
strip ${BUILDDIR}/gdb/gdb -g -S -d --strip-debug --strip-dw --strip-unneeded  -s
strip ${LIBPATH}/libcngdb_backend.so -g -S -d --strip-debug --strip-dw --strip-unneeded  -s

make prefix=$INSTALLDIR install

mv $INSTALLDIR/bin/gdb $INSTALLDIR/bin/cngdb

popd
