.syntax unified
.arch armv6k
.arm
.global __sync_synchronize_dmb
.global __dmb

__dmb:
__sync_synchronize_dmb:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 5
	bx lr