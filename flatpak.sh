#!/bin/sh

set -e -x

cd _flatpak
manifest=../org.gnome.PurpleEgg.json
stamp=install-base-stamp
if [ -e $stamp -a $stamp -nt $manifest ] ; then
    :
else
    rm -rf install-base && mkdir -p install-base
    flatpak-builder --stop-at=PurpleEgg install-base $manifest
    touch install-base-stamp
fi

rm -rf install
cp -al install-base install
mkdir -p build
flatpak build --build-dir=$(pwd)/build install sh -c '( [ -e Makefile ] ||../../configure --prefix=/app ) && make && make install'
flatpak-builder --repo=repo --finish-only --disable-cache install ../org.gnome.PurpleEgg.json

flatpak --user uninstall org.gnome.PurpleEgg > /dev/null || true
flatpak --user remote-add --if-not-exists --no-gpg-verify PurpleEgg-devel $(pwd)/_flatpak/repo
flatpak --user install PurpleEgg-devel org.gnome.PurpleEgg





