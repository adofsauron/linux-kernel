#!/bin/bash
#
# install.sh -- install one directly or download it at first and then install one, only accept deb currently
#

pkgs="$1"

[ -z "$pkgs" ] && exit 0

CACHED_DIR=$(cd $(dirname $0) && pwd)/cached

[ ! -d $CACHED_DIR ] && mkdir -p $CACHED_DIR

for p in $pkgs
do
  cmd=$(echo "$p" | cut -d ';' -f1)
  pkg=$(echo "$p" | cut -d ';' -f2)
  version=$(echo "$p" | cut -d ';' -f3)

  # ignore cmd already installed
  if [ -z "$version" ]; then
    which $cmd >/dev/null 2>&1
    [ $? -eq 0 ] && continue
  else
    which $cmd-$version >/dev/null 2>&1
    [ $? -eq 0 ] && continue
  fi

  echo "$pkg" | egrep -iq "http|ftp"
  if [ $? -eq 0 ]; then
    echo $pkg | grep ".deb"
    if [ $? -eq 0 ]; then
      wget -c $pkg -P $CACHED_DIR
      [ $? -ne 0 ] && echo "ERR: Failed to download $pkg" && continue

      # backup old cmd if version specified
      if [ -n "$version" ]; then
        tmp_cmd=`mktemp`
        old_cmd=`which $cmd`
        sudo cp $old_cmd $tmp_cmd
      fi

      sudo dpkg -i $CACHED_DIR/$(basename $pkg)

      if [ $? -eq 0 -a -n "$version" ]; then
        new_cmd=`which $cmd`
        sudo mv $new_cmd $new_cmd-$version
        sudo mv $tmp_cmd $old_cmd
        sudo chmod a+x $old_cmd $new_cmd-$version
      else
        echo "ERR: failed to install $pkg" && continue
      fi
    else
      echo "ERR: only support deb package currently, $pkg is not supported."
    fi
  else
    sudo apt-get update -y && sudo apt-get install -y $pkg
  fi
done
