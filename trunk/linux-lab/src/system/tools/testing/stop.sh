#!/bin/sh
#
# stop.sh -- stop testing of a kernel feature
#
# Copyright (C) 2016-2021 Wu Zhangjin <falcon@ruma.tech>
#

[ -r /etc/default/testing ] && . /etc/default/testing

# Get feature list from kernel command line
FEATURE="$(cat /proc/cmdline | tr ' ' '\n' | grep ^feature= | cut -d'=' -f2 | tr ',' ' ')"
[ -z "$FEATURE" ] && exit 0

echo
echo "Stop testing ..."
echo

# Dummy

echo "OK"
