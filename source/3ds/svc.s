.arm
.align 4

.macro BEGIN_ASM_FUNC name, linkage=global, section=text
    .section        .\section\().\name, "ax", %progbits
    .align          2
    .\linkage       \name
    .type           \name, %function
    .func           \name
    .cfi_sections   .debug_frame
    .cfi_startproc
    \name:
.endm

.macro END_ASM_FUNC
    .cfi_endproc
    .endfunc
.endm

BEGIN_ASM_FUNC svcControlMemory
	push {r0, r4}
	ldr  r0, [sp, #0x8]
	ldr  r4, [sp, #0x8+0x4]
	svc  0x01
	ldr  r2, [sp], #4
	str  r1, [r2]
	ldr  r4, [sp], #4
	bx   lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcGetResourceLimitCurrentValues
	svc 0x3A
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcGetResourceLimitLimitValues
	svc 0x39
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcGetResourceLimit
	str r0, [sp, #-0x4]!
	svc 0x38
	ldr r3, [sp], #4
	str r1, [r3]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCloseHandle
	svc 0x23
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcWaitSynchronization
	svc 0x24
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcWaitSynchronizationN
	str r5, [sp, #-4]!
	str r4, [sp, #-4]!
	mov r5, r0
	ldr r0, [sp, #0x8]
	ldr r4, [sp, #0x8+0x4]
	svc 0x25
	str r1, [r5]
	ldr r4, [sp], #4
	ldr r5, [sp], #4
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcBreak
	svc 0x3C
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcConnectToPort
	str r0, [sp, #-0x4]!
	svc 0x2D
	ldr r3, [sp], #4
	str r1, [r3]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcSendSyncRequest
	svc 0x32
	bx lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcSleepThread
	svc 0x0A
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcGetProcessId
	str r0, [sp, #-0x4]!
	svc 0x35
	ldr r3, [sp], #4
	str r1, [r3]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcReplyAndReceive
	str r0, [sp, #-4]!
	svc 0x4F
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcAcceptSession
	str r0, [sp, #-4]!
	svc 0x4A
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreateThread
	push {r0, r4}
	ldr  r0, [sp, #0x8]
	ldr  r4, [sp, #0x8+0x4]
	svc  0x08
	ldr  r2, [sp], #4
	str  r1, [r2]
	ldr  r4, [sp], #4
	bx   lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreateAddressArbiter
	push {r0}
	svc 0x21
	pop {r2}
	str r1, [r2]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcArbitrateAddressNoTimeout
	svc 0x22
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreatePort
	push {r0, r1}
	svc 0x47
	ldr r3, [sp, #0]
	str r1, [r3]
	ldr r3, [sp, #4]
	str r2, [r3]
	add sp, sp, #8
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreateSessionToPort
	push {r0}
	svc 0x48
	pop {r2}
	str r1, [r2]
	bx lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcOutputDebugString
	svc 0x3D
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreateMutex
	str r0, [sp, #-4]!
	svc 0x13
	ldr r3, [sp], #4
	str r1, [r3]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcCreateEvent
	str r0, [sp, #-4]!
	svc 0x17
	ldr r2, [sp], #4
	str r1, [r2]
	bx  lr
END_ASM_FUNC

BEGIN_ASM_FUNC svcSignalEvent
	svc 0x18
	bx  lr
END_ASM_FUNC