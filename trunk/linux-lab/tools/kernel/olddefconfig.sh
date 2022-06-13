#!/bin/bash
#
# olddefconfig.sh -- get available olddefconfig target
#
# Copyright (C) 2016-2021 Wu Zhangjin <falcon@ruma.tech>
#

makefile=$1

oldconfig=olddefconfig

if [ -n "$makefile" -a -f "$makefile" ]; then
  grep olddefconfig -q $makefile
  if [ $? -ne 0 ]; then
    grep oldnoconfig -q $makefile
    if [ $? -ne 0 ]; then
      oldconfig=oldconfig
    else
      oldconfig=oldnoconfig
    fi
  fi
fi

echo $oldconfig
