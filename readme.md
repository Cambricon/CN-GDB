 # CAMBRICON CN-GDB

This is CN-GDB, the BANG-C source-level debugger on Linux, based on GDB-7.11, the GNU source-level debugger.

> For more information about gdb, please refer to the README file in this folder or check the GDB home page at [http://www.gnu.org/software/gdb](http://www.gnu.org/software/gdb) .

## HOW TO BUILD CN-GDB

cngdb dependence as follows:

> from gdb-root/debian/control

```
# Packaging deps
               cdbs (>= 0.4.90),
               debhelper (>= 9),
               lsb-release,
               bzip2,
# Other tool deps
               autoconf,
               libtool,
               gettext,
               bison,
               dejagnu,
               flex | flex-old,
               procps,
               g++-multilib [i386 powerpc s390 sparc],
               gcj-jdk,
               mig [hurd-any],
# TeX[info] deps
               texinfo (>= 4.7-2.2),
               texlive-base,
# Libdev deps
               libexpat1-dev, lib64expat1-dev [i386 powerpc s390 sparc],
               libncurses5-dev, lib64ncurses5-dev [i386 powerpc s390 sparc],
               libreadline-dev, lib64readline6-dev [i386 powerpc s390 sparc],
               zlib1g-dev,
               liblzma-dev,
               libbabeltrace-dev [amd64 armel armhf i386 kfreebsd-amd64 kfreebsd-i386 mips mipsel powerpc ppc64 ppc64el s390x],
               libbabeltrace-ctf-dev [amd64 armel armhf i386 kfreebsd-amd64 kfreebsd-i386 mips mipsel powerpc ppc64 ppc64el s390x],
               python3-dev,
               libkvm-dev [kfreebsd-any],
               libunwind7-dev [ia64],
# gdb64
               lib64z1-dev [i386 powerpc],
               lib64expat1-dev [i386 powerpc],
# gdb-doc
               texlive-latex-base, texlive-fonts-recommended, cm-super,
```

You can compile `cngdb` using the provided build script `build.sh`, The instructions are as follows:

```bash
     cd ${gdb-root}
    ./build.sh
```

After compilation, executable `cngdb` will be in the path `gdb-root/build/neuware/bin/cngdb`

## HOW TO USE CN-GDB

Nearly all standard GDB commands could be used both for debugging on CPU and MLU code.
In addition to that, `cngdb` provides specific command families like `cngdb info...` to query MLU device states, `cngdb focus ...` to control debugger focus.

For more information please check `cngdb` user manual.

## REPORT BUG

Send e-mail to [compiler-devel@cambricon.com](mailto:compiler-devel@cambricon.com) to report cngdb bug.

or post on the [Cambricon Developer Forum](http://forum.cambricon.com/list-21-1.html)
