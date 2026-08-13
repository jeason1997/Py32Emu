.syntax unified
.cpu cortex-m0plus
.thumb

.global trigger_undefined_instruction
.type trigger_undefined_instruction,%function
trigger_undefined_instruction:
    .hword 0xde00
    bx lr
.size trigger_undefined_instruction, .-trigger_undefined_instruction

.global trigger_unaligned_load
.type trigger_unaligned_load,%function
trigger_unaligned_load:
    ldr r0, =0x20000001
    ldr r0, [r0]
    bx lr
.size trigger_unaligned_load, .-trigger_unaligned_load

.global SVC_Handler
.type SVC_Handler,%function
SVC_Handler:
    push {r4}
    mov r4, lr
    ldr r0, =0xfffffff5
    bx r0
    mov lr, r4
    pop {r4}
    bx lr
.size SVC_Handler, .-SVC_Handler
