.syntax unified
.cpu cortex-m0plus
.thumb

.global g_pfnVectors
.global Reset_Handler

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler,%function
Reset_Handler:
    ldr r0, =_estack
    mov sp, r0
    bl SystemInit
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
1:  cmp r0, r1
    bcs 2f
    ldr r3, [r2]
    str r3, [r0]
    adds r0, #4
    adds r2, #4
    b 1b
2:  ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
3:  cmp r0, r1
    bcs 4f
    str r2, [r0]
    adds r0, #4
    b 3b
4:  bl main
5:  b 5b
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
Default_Handler:
    b Default_Handler

.macro weak_handler name
    .weak \name
    .thumb_set \name, Default_Handler
.endm

weak_handler NMI_Handler
weak_handler HardFault_Handler
weak_handler SVC_Handler
weak_handler PendSV_Handler
weak_handler SysTick_Handler
weak_handler FLASH_IRQHandler
weak_handler RCC_IRQHandler
weak_handler EXTI0_1_IRQHandler
weak_handler EXTI2_3_IRQHandler
weak_handler EXTI4_15_IRQHandler
weak_handler DMA1_Channel1_IRQHandler
weak_handler ADC_COMP_IRQHandler
weak_handler TIM1_BRK_UP_TRG_COM_IRQHandler
weak_handler TIM1_CC_IRQHandler
weak_handler LPTIM1_IRQHandler
weak_handler TIM16_IRQHandler
weak_handler I2C1_IRQHandler
weak_handler SPI1_IRQHandler
weak_handler USART1_IRQHandler

.section .isr_vector,"a",%progbits
.align 2
g_pfnVectors:
    .word _estack, Reset_Handler, NMI_Handler, HardFault_Handler
    .word 0, 0, 0, 0, 0, 0, 0, SVC_Handler, 0, 0
    .word PendSV_Handler, SysTick_Handler
    .word 0, 0, 0, FLASH_IRQHandler, RCC_IRQHandler
    .word EXTI0_1_IRQHandler, EXTI2_3_IRQHandler, EXTI4_15_IRQHandler
    .word 0, DMA1_Channel1_IRQHandler, 0, 0, ADC_COMP_IRQHandler
    .word TIM1_BRK_UP_TRG_COM_IRQHandler, TIM1_CC_IRQHandler, 0, 0
    .word LPTIM1_IRQHandler, 0, 0, 0, TIM16_IRQHandler, 0
    .word I2C1_IRQHandler, 0, SPI1_IRQHandler, 0, USART1_IRQHandler
    .word 0, 0, 0, 0

