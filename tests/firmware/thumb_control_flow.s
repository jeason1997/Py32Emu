.syntax unified
.cpu cortex-m0plus
.thumb

.global thumb_control_flow_test
.type thumb_control_flow_test,%function
thumb_control_flow_test:
    push {lr}
    yield
    sev
    wfe
    movs r0, #1
    movs r1, #1
    rors r0, r1
    movs r2, #1
    lsls r2, r2, #31
    cmp r0, r2
    bne 1f
    svc #7
    ldr r0, =svc_seen
    ldr r0, [r0]
    cmp r0, #1
    bne 1f
    movs r0, #1
    pop {pc}
1:
    movs r0, #0
    pop {pc}
.size thumb_control_flow_test, .-thumb_control_flow_test
