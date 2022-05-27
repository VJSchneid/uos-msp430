.sect   .text
    .global thread_switch

thread_switch:
pushm.w #7, R10  ; store callee saved registers (R4-R10)
mov.w   SP, @R13 ; move function argument (new SP) to address of first argument
mov.w   @R12, SP  ; restore new SP
popm.w  #7, R10  ; restore all callee stored registers from stack
ret
