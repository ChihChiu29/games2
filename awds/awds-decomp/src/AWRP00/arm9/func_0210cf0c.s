/*
 * Assembly is required for this unit: no recoverable MWCC build reproduces
 * the target's Thumb codegen from C. See docs/TOOLCHAIN.md.
 */

	.text
	.thumb
	.global func_0210cf0c
	.type func_0210cf0c, @function

func_0210cf0c:
	str r1, [r0, #0x14]
	neg r1, r1
	str r1, [r0, #0x1c]
	mov r1, #1
	lsl r1, r1, #12
	stmia r0!, {r1}
	mov r3, #0
	mov r1, #0
	stmia r0!, {r1, r3}
	stmia r0!, {r1, r2}
	str r1, [r0, #4]
	add r0, #12
	stmia r0!, {r2, r3}
	stmia r0!, {r1, r3}
	bx lr

	.size func_0210cf0c, 30
