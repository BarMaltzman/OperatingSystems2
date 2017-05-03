#include "syscall.h"
#include "traps.h"

.globl ret_start
.globl ret_end
ret_start:
    movl $SYS_sigreturn, %eax
    int $T_SYSCALL
ret_end:
