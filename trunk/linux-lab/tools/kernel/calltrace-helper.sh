#!/bin/bash
#
# Copyright (C) 2016-2021 Wu Zhangjin <falcon@ruma.tech>
# Author: Wu Zhangjin <wuzhangjin@gmail.com>, wechat: lzufalcon, 2019-05-28
#
# calltrace-helper.sh -- analyze calltrace result, find out the issue lines and changes.
#
# Usage: calltrace-helper.sh vmlinux_debuginfo calltrace_lastcall kernel_gitsrc cc_path cc_pre
#
#        If addr2line=faddr2line specified, will use scripts/faddr2line instead of raw addr2line.
#
# ref: http://tinylab.org/find-out-the-code-line-of-kernel-panic-address/
#
# TODO:
#
# 1. add auto-git-blame support, find out who and which version changes this line`
# 2. add auto-git-bisect support, find out which version is the first bad version
#

#
### Kernel must be compiled with debug info and symbols:
#
#   CONFIG_DEBUG_KERNEL=y
#   CONFIG_DEBUG_INFO=y
#   CONFIG_KALLSYMS=y
#   CONFIG_KALLSYMS_ALL=y
#   CONFIG_DEBUG_BUGVERBOSE=y
#   CONFIG_STACKTRACE=y
#
### Reconfigure the kernel with debug support:
#
# $ make f f=debug
# $ make kernel-olddefconfig
# $ make kernel

#
### Calltrace example
#
# [   26.292778] WARNING: CPU: 0 PID: 187 at kernel/sched/core.c:1211 set_task_cpu+0x1f0/0x200
# [   26.296252] Modules linked in: [last unloaded: hello]
# [   26.298465] CPU: 0 PID: 187 Comm: init Tainted: P        W  O      5.1.0-v8+ #4
# [   26.302369] Hardware name: Raspberry Pi 3 Model B (DT)
# [   26.304549] pstate: 20000085 (nzCv daIf -PAN -UAO)
# [   26.306528] pc : set_task_cpu+0x1f0/0x200
# [   26.308187] lr : try_to_wake_up+0x188/0x4a0
# [   26.309905] sp : ffffff80110cbac0
# [   26.311355] x29: ffffff80110cbac0 x28: ffffffc038b1d7c0
# [   26.313641] x27: ffffff8010d0d3f8 x26: ffffff8010b4ad00
# [   26.315894] x25: 0000000000000001 x24: ffffffc039d583a0
# [   26.316587] x23: 0000000000000080 x22: ffffffc039d58784
# [   26.319699] x21: 0000000000000004 x20: 0000000000000001
# [   26.320202] x19: ffffffc039d58000 x18: 0000000000000010
# [   26.320719] x17: 0000000000000000 x16: 0000000000000000
# [   26.321314] x15: ffffffffffffffff x14: ffffff8010d0c688
# [   26.321847] x13: ffffff80910cba67 x12: ffffff80110cba6f
# [   26.322424] x11: ffffff8010d20000 x10: ffffff80110cb9f0
# [   26.322992] x9 : 00000000ffffffd0 x8 : 0000000000046f76
# [   26.323596] x7 : 00000000fffffaaa x6 : 0000000014c5ebc7
# [   26.324192] x5 : 00ffffffffffffff x4 : 0000000000000015
# [   26.325014] x3 : 0000000001b87190 x2 : 0000000000000001
# [   26.325758] x1 : ffffff8010d0c6b8 x0 : 0000000000000000
# [   26.326368] Call trace:
# [   26.326693]  set_task_cpu+0x1f0/0x200
# [   26.327152]  try_to_wake_up+0x188/0x4a0
# [   26.327753]  default_wake_function+0x34/0x48
# [   26.328116]  __wake_up_common+0x90/0x150
# [   26.328443]  __wake_up_locked+0x40/0x50
# [   26.328776]  complete+0x50/0x70
# [   26.329093]  mm_release+0x84/0x138
# [   26.329382]  do_exit+0x2dc/0xa28
# [   26.329582]  __se_sys_reboot+0x168/0x210
# [   26.329800]  __arm64_sys_reboot+0x24/0x30
# [   26.330030]  el0_svc_common.constprop.0+0x8c/0x110
# [   26.330298]  el0_svc_handler+0x34/0x90
# [   26.330509]  el0_svc+0x8/0xc
# [   26.330708] ---[ end trace 183c6d86daad5ed8 ]---

# Last call is set_task_cpu+0x1f0/0x200

#
# example:
#
# // enable debugging support
# $ make f f=debug
# $ make kernel
#
# // dump the assembly with source embedded
# $ make kernel-calltrace lastcall=set_task_cpu+0x1f0/0x200
#

AFTER_LINES=8
BEFORE_LINES=8

vmlinux_debuginfo=$1
calltrace_lastcall=$2
kernel_gitsrc=$3
cc_path="$4"
cc_pre="$5"

[ -z "$calltrace_lastcall" -o -z "$vmlinux_debuginfo" -o -z "$kernel_gitsrc" ] && \
  echo "Usage: $0 vmlinux_debuginfo calltrace_lastcall kernel_gitsrc cc_path cc_pre" && exit 1

cc_path_pre="$cc_path $cc_pre"

# for KASLR
faddr2line=${kernel_gitsrc}/scripts/faddr2line
cc_faddr2line="$cc_path CROSS_COMPILE=${cc_pre} ${faddr2line}"

echo
echo "[ addr2line ]:"
echo

if [ -n "${addr2line}" -a "${addr2line}" = "faddr2line" -a -f ${faddr2line} ]; then

  eval ${cc_faddr2line} ${vmlinux_debuginfo} ${calltrace_lastcall}
  file_line=$(eval ${cc_faddr2line} ${vmlinux_debuginfo} ${calltrace_lastcall} | tail -1 | cut -d' ' -f3)

else

  cc_nm=${cc_path_pre}nm
  cc_addr2line=${cc_path_pre}addr2line
  cc_gdb=${cc_path_pre}gdb
  cc_objdump=${cc_path_pre}objdump

  func=$(echo $calltrace_lastcall | cut -d'+' -f1)

  echo ${cc_nm} ${vmlinux_debuginfo}

  addr=$(${cc_nm} ${vmlinux_debuginfo} | grep " [Tt] ${func}$" | cut -d' ' -f1)

  echo "func: $func addr: $addr"

  err_offset=$(echo $calltrace_lastcall | cut -d'+' -f2 | cut -d'/' -f1)
  func_len=$(echo $calltrace_lastcall | cut -d'+' -f2 | cut -d'/' -f2)

  echo "offset: $err_offset len: $func_len"

  startaddr=${addr}

  addrprefix=$(echo ${addr} | sed -e "s#\(f\{1,\}\).*#\1#g")
  addrreal=$(echo ${addr} | sed -e "s/^f\{1,\}\([^f]*\)/\1/g")

  echo "prefix: $addrprefix real: $addrreal"

  stopaddr=${addrprefix}$(echo "obase=16;ibase=10;$((0x$addrreal+$func_len))" | bc)
  erraddr=${addrprefix}$(echo "obase=16;ibase=10;$((0x$addrreal+$err_offset))" | bc)

  echo "start: $startaddr stop: $stopaddr err: $erraddr"

  ${cc_addr2line} -e ${vmlinux_debuginfo} ${erraddr}
  file_line=$(${cc_addr2line} -e ${vmlinux_debuginfo} ${erraddr} | cut -d' ' -f1)

  echo
  echo "[   objdump ]:"
  echo

  ${cc_objdump} -dlS ${vmlinux_debuginfo} --start-address=0x${startaddr} --stop-address=0x${stopaddr} \
    | grep -i --color=always -A $AFTER_LINES -B $BEFORE_LINES ${erraddr}

  echo
  echo "[      gdb  ]:"
  echo
  ${cc_gdb} -nh -nx -q -ex "list *(0x$erraddr)" -ex "quit" ${vmlinux_debuginfo}

fi

echo
echo "[ git blame ]:"
echo

line=$(echo $file_line | cut -d ':' -f2)
file=$(echo $file_line | cut -d ':' -f1)
file=$(echo $file | sed -e "s%${kernel_gitsrc}/%%g")

pushd $kernel_gitsrc >/dev/null
blame_info=$(git blame -L $line,$line $file)
blame_commit=$(echo $blame_info | cut -d' ' -f1)
echo $blame_info
echo
git show $blame_commit
echo
popd >/dev/null
