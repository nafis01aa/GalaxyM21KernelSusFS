#!/bin/bash
set -e

export ARCH=arm64
export ANDROID_MAJOR_VERSION=q

WORKSPACE=$(pwd)

export PATH=$WORKSPACE/toolchains/gcc/bin:$WORKSPACE/toolchains/clang/clang-4691093/bin:$PATH

TOOLCHAIN_ARGS="CROSS_COMPILE=$WORKSPACE/toolchains/gcc/bin/aarch64-linux-android- \
CC=$WORKSPACE/toolchains/clang/clang-4691093/bin/clang \
CLANG_TRIPLE=aarch64-linux-gnu- \
HOSTCFLAGS=-fcommon -Wno-error \
KCFLAGS=-Wno-error"

echo "==== Applying defconfig ===="
make $TOOLCHAIN_ARGS clean
make $TOOLCHAIN_ARGS exynos9610-m21dd_defconfig

echo "==== Building Kernel ===="
make $TOOLCHAIN_ARGS -j$(nproc)

echo "==== Build finished ===="
if [ -f arch/arm64/boot/Image ]; then
    echo "SUCCESS: Kernel Image built at arch/arm64/boot/Image"
else
    echo "FAILED: No Image produced"
    exit 1
fi

# ==== NEW: Repack boot.img ====
echo "==== Packing boot.img ===="
IMGS=/root/imgs
IMAGE="$WORKSPACE/arch/arm64/boot/Image"

[ -f "$IMGS/stock.img" ]   || { echo "FAILED: $IMGS/stock.img missing"; exit 1; }
[ -f "$IMGS/magiskboot" ]  || { echo "FAILED: $IMGS/magiskboot missing"; exit 1; }
chmod +x "$IMGS/magiskboot"

# Throwaway work dir: unpack leftovers (kernel, dtb, ramdisk...) vanish with it
PACK_DIR=$(mktemp -d /tmp/bootpack.XXXX)
trap 'rm -rf "$PACK_DIR"' EXIT

cp "$IMGS/magiskboot" "$PACK_DIR/"
cp "$IMGS/stock.img"  "$PACK_DIR/boot.img"   # stock.img stays untouched
cp "$IMAGE"           "$PACK_DIR/Image"

(
    cd "$PACK_DIR"
    ./magiskboot unpack boot.img
    mv -f Image kernel
    ./magiskboot repack boot.img
)

mv -f "$PACK_DIR/new-boot.img" "$IMGS/boot.img"
echo "SUCCESS: New boot image at $IMGS/boot.img ($(du -h "$IMGS/boot.img" | cut -f1))"

# ==== NEW: Make Odin-flashable tar ====
echo "==== Creating boot.tar ===="
rm -f "$IMGS/boot.tar"
tar -H ustar -C "$IMGS" -cf "$IMGS/boot.tar" boot.img
echo "SUCCESS: Odin tar at $IMGS/boot.tar ($(du -h "$IMGS/boot.tar" | cut -f1))"