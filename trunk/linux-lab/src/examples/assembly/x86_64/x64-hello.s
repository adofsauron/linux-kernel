# ref: http://cs.lmu.edu/~ray/notes/gasexamples/
# ref: arch/x86/include/asm/unistd_64.h

# ----------------------------------------------------------------------------------------
# Writes "Hello, World" to the console using only system calls. Runs on 64-bit Linux only.
# To assemble and run:
#
#     gcc -c hello.s && ld hello.o && ./a.out
#
# or
#
#     gcc -nostdlib hello.s && ./a.out
# ----------------------------------------------------------------------------------------

    .text
    .global _start
_start:
                          # write(1, msg, len)
    mov     $1, %rdi      # file handle 1 is stdout
    mov     $msg, %rsi    # address of string to output
    mov     $len, %rdx    # number of bytes

    mov     $1, %rax      # system call 1 is write
    syscall               # invoke operating system to do the write

                          # exit(0)
    mov     $60, %rax     # system call 60 is exit
    xor     %rdi, %rdi    # we want return code 0
    syscall               # invoke operating system to exit

    .section .rodata
msg:
    .ascii  "Hello, X86_64!\n"
    len = . - msg         # length of our dear string
