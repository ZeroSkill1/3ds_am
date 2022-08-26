    .section .text._thread_start, "ax", %progbits
    .align  2
    .global _thread_start
    .type   _thread_start, %function
_thread_start:
    ldmdb   sp, {r0,r1} @ pop function and argument
    blx     r1 @ call function with argument set
    svc     0x09