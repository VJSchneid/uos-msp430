.sect   .text
    .global thread_switch

thread_switch:
pushm.a #7, R10  ; store callee saved registers (R4-R10)
mov.a   SP, @R13 ; move function argument (new SP) to address of first argument
mov.a   @R12, SP  ; restore new SP
popm.a  #7, R10  ; restore all callee stored registers from stack
ret
