.syntax unified
.cpu cortex-m0plus
.thumb
.global thumb_register_edges_test
.type thumb_register_edges_test,%function
thumb_register_edges_test:
    push {r4,lr}
    sub sp,#8
    movs r0,#17
    str r0,[sp]
    movs r0,#23
    str r0,[sp,#4]
    mov r2,sp
    ldmia r2,{r2,r3}
    cmp r2,#17
    bne 9f
    cmp r3,#23
    bne 9f
    .balign 4
    adr r1,1f
    nop
    mov r0,pc
    nop
1:
    cmp r0,r1
    bne 9f
    mrs r0,epsr
    cmp r0,#0
    bne 9f
    movs r0,#1
    lsls r0,r0,#30
    msr APSR_nzcvq,r0
    mrs r1,xpsr
    cmp r0,r1
    bne 9f
    movs r0,#1
    b 10f
9:
    movs r0,#0
10:
    add sp,#8
    pop {r4,pc}
.size thumb_register_edges_test,.-thumb_register_edges_test
