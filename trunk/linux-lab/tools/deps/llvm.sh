#!/bin/bash
#
# llvm.sh
#

# version 13 is verified for rust-for-linux compiling
V=$1
[ -z "$V" ] && V=13

wget -O - https://apt.llvm.org/llvm.sh | sudo bash -s -- $V

for t in clang clang-cpp clangd clang++ ld.lld
do
  sudo ln -sf /usr/bin/$t-$V /usr/bin/$t
done

for t in ar as addr2line lto lto2 nm objcopy objdump ranlib readelf size strings strip
do
  sudo ln -sf /usr/bin/llvm-$t-$V /usr/bin/llvm-$t
done
