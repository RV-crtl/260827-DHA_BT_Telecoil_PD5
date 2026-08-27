.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global g_pfnVectors
.global Reset_Handler
.extern main
.extern _estack
.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler, %function
Reset_Handler:
    /* Enable CP10/CP11 so hard-float code may execute before entering C. */
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    ldr r2, =0x00F00000
    orr r1, r1, r2
    str r1, [r0]
    dsb
    isb

    /* Copy .data from Flash to SRAM. */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
1:
    cmp r0, r1
    bcc 2f
    b 3f
2:
    ldr r3, [r2], #4
    str r3, [r0], #4
    b 1b

    /* Zero .bss. */
3:
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
4:
    cmp r0, r1
    bcc 5f
    b 6f
5:
    str r2, [r0], #4
    b 4b

6:
    bl main
7:
    b 7b
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
Default_Handler:
    b Default_Handler
.size Default_Handler, .-Default_Handler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    /* Cortex-M4 exception vectors (NMI through SysTick). */
    .rept 14
    .word Default_Handler
    .endr
    /* STM32F446 external IRQ vectors. Extra default entries are harmless. */
    .rept 128
    .word Default_Handler
    .endr
.size g_pfnVectors, .-g_pfnVectors
