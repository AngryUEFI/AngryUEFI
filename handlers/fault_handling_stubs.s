    .section .text

    .align 16
    .global proto_fault_stub_start
proto_fault_stub_start:
    .type proto_fault_stub_context_ptr, STT_OBJECT
# replaced during init with ptr to core context
proto_fault_stub_context_ptr:
    .quad 0x0
    .quad 0x0 # pad

# expects stack layout as:
# old_rsp
# old_rax
# old rbx
# ret address <- RSP
# 
# expects CoreFaultInfo* in RAX
store_registers:
    # write rcx so we have another scratch reg
    movq %rcx, 0x30(%rax)

    # original rax
    movq 0x10(%rsp), %rcx
    movq %rcx, 0x20(%rax)
    # original rbx
    movq 0x8(%rsp), %rcx
    movq %rcx, 0x28(%rax)
    # original rsp on entry to ISR
    movq 0x18(%rsp), %rcx
    movq %rcx, 0x50(%rax)

    # rax already written
    # rbx already written
    # rcx already written
    movq %rdx, 0x38(%rax)
    movq %rsi, 0x40(%rax)
    movq %rdi, 0x48(%rax)
    # rsp already written
    movq %rbp, 0x58(%rax)
    movq %r8, 0x60(%rax)
    movq %r9, 0x68(%rax)
    movq %r10, 0x70(%rax)
    movq %r11, 0x78(%rax)
    movq %r12, 0x80(%rax)
    movq %r13, 0x88(%rax)
    movq %r14, 0x90(%rax)
    movq %r15, 0x98(%rax)

    mov %cr0, %rbx
    movq %rbx, 0xA8(%rax)
    mov %cr2, %rbx
    movq %rbx, 0xB0(%rax)
    mov %cr3, %rbx
    movq %rbx, 0xB8(%rax)
    mov %cr4, %rbx
    movq %rbx, 0xC0(%rax)
    ret

get_core_ctx_ptr:
    movq proto_fault_stub_context_ptr(%rip), %rax
    ret

    # fault handlers "retrun" here
dummy_loop:
    jmp dummy_loop

.macro m_handler_inner handler_id
    # we want to know the original rsp value
    push %rsp
    # scratch regs
    push %rax
    push %rbx

    # load ptr to core context
    call get_core_ctx_ptr

    # backup location of core context into %rbx
    mov %rax, %rbx

    # get CoreFaultInfo address
    movq 72(%rax), %rax
    call store_registers

    # fault occured = 1
    movq $1, 0(%rax)

    # fault number
    movq $\handler_id, 0x8(%rax)

    # restore dirtied registers
    popq %rbx
    popq %rax
    popq %rsp

.endm

    # stack layout:
    # original RSP
    # RFLAGS
    # CS
    # RIP <- RSP
.macro m_handler_no_error_code handler_id
    m_handler_inner \handler_id

    # old RIP
    movq 0(%rsp), %rcx
    movq %rcx, 0x18(%rax)

    # CS
    movq 0x8(%rsp), %rcx
    movq %rcx, 0xC8(%rax)

    # RFLAGS
    movq 0x10(%rsp), %rcx
    movq %rcx, 0xA0(%rax)

    # original RSP
    movq 0x18(%rsp), %rcx
    movq %rcx, 0xD0(%rax)

    lea dummy_loop(%rip), %rax
    movq %rax, (%rsp)
    iretq

.endm

    # stack layout:
    # /error code (# error is a macro)
    # original RSP
    # RFLAGS
    # CS
    # RIP <- RSP
.macro m_handler_with_error_code handler_id
    m_handler_inner \handler_id

    # /error code
    movq 0(%rsp), %rcx
    movq %rcx, 0x10(%rax)
    
    # old RIP
    movq 0x8(%rsp), %rcx
    movq %rcx, 0x18(%rax)

    # CS
    movq 0x10(%rsp), %rcx
    movq %rcx, 0xC8(%rax)

    # RFLAGS
    movq 0x18(%rsp), %rcx
    movq %rcx, 0xA0(%rax)

    # original RSP
    movq 0x20(%rsp), %rcx
    movq %rcx, 0xD0(%rax)

    popq %rax
    lea dummy_loop(%rip), %rax
    movq %rax, (%rsp)
    iretq

.endm

    .align 16
proto_handler_0:
    m_handler_no_error_code 1

    .align 16
proto_handler_1:
    m_handler_no_error_code 1

    .align 16
proto_handler_2:
    m_handler_no_error_code 2

    .align 16
proto_handler_3:
    m_handler_no_error_code 3

    .align 16
proto_handler_4:
    m_handler_no_error_code 4

    .align 16
proto_handler_5:
    m_handler_no_error_code 5

    .align 16
proto_handler_6:
    m_handler_no_error_code 6

    .align 16
proto_handler_7:
    m_handler_no_error_code 7

    .align 16
proto_handler_8:
    # double fault
    # this must always lock up the core
    # even if later other handlers allow resume
    m_handler_inner 8

    # /error code
    movq 0(%rsp), %rcx
    movq %rcx, 0x10(%rax)
    
    # old RIP
    movq 0x8(%rsp), %rcx
    movq %rcx, 0x18(%rax)

    # CS
    movq 0x10(%rsp), %rcx
    movq %rcx, 0xC8(%rax)

    # RFLAGS
    movq 0x18(%rsp), %rcx
    movq %rcx, 0xA0(%rax)

    # original RSP
    movq 0x20(%rsp), %rcx
    movq %rcx, 0xD0(%rax)

    lea dummy_loop(%rip), %rax
    movq %rax, (%rsp)
    iretq

    .align 16
proto_handler_9:
    m_handler_no_error_code 9

    .align 16
proto_handler_10:
    m_handler_with_error_code 10

    .align 16
proto_handler_11:
    m_handler_with_error_code 11

    .align 16
proto_handler_12:
    m_handler_with_error_code 12

    .align 16
proto_handler_13:
    # the GPF handler
    # failed ucode updates trigger a GPF
    # we try to resume, this might cause another fault down the line though

    m_handler_inner 13

    # /error code
    movq 0(%rsp), %rcx
    movq %rcx, 0x10(%rax)
    
    # old RIP
    movq 0x8(%rsp), %rcx
    movq %rcx, 0x18(%rax)

    # CS
    movq 0x10(%rsp), %rcx
    movq %rcx, 0xC8(%rax)

    # RFLAGS
    movq 0x18(%rsp), %rcx
    movq %rcx, 0xA0(%rax)

    # original RSP
    movq 0x20(%rsp), %rcx
    movq %rcx, 0xD0(%rax)

    # remove error code from stack
    popq %rax
    # load retrun RIP
    popq %rax
    # add 2 byte, size of wrmsr
    add $2, %rax
    # put return RIP back onto stack
    push %rax

    # put signal value into rax
    mov $0xDEAD, %rax
    iretq

    .align 16
proto_handler_14:
    m_handler_with_error_code 14

    .align 16
proto_handler_15:
    m_handler_no_error_code 15

    .align 16
proto_handler_16:
    m_handler_no_error_code 16

    .align 16
proto_handler_17:
    m_handler_with_error_code 17

    .align 16
proto_handler_18:
    m_handler_no_error_code 18

    .align 16
proto_handler_19:
    m_handler_no_error_code 19

    .align 16
proto_handler_20:
    m_handler_no_error_code 20

    .align 16
proto_handler_21:
    m_handler_with_error_code 21

    .align 16
    .global fallback_handler
    .type fallback_handler, STT_FUNC
fallback_handler:
    m_handler_no_error_code 0x100

    .align 16
    .global proto_handlers_end
    .type proto_handlers_end, STT_OBJECT
proto_handlers_end:
    .quad 0xdeadbeefdeadc0de
    .global proto_fault_handlers_offsets
    .type proto_fault_handlers_offsets, STT_OBJECT
proto_fault_handlers_offsets:
    .quad proto_handler_0-proto_fault_stub_start
    .quad proto_handler_1-proto_fault_stub_start
    .quad proto_handler_2-proto_fault_stub_start
    .quad proto_handler_3-proto_fault_stub_start
    .quad proto_handler_4-proto_fault_stub_start
    .quad proto_handler_5-proto_fault_stub_start
    .quad proto_handler_6-proto_fault_stub_start
    .quad proto_handler_7-proto_fault_stub_start
    .quad proto_handler_8-proto_fault_stub_start
    .quad proto_handler_9-proto_fault_stub_start
    .quad proto_handler_10-proto_fault_stub_start
    .quad proto_handler_11-proto_fault_stub_start
    .quad proto_handler_12-proto_fault_stub_start
    .quad proto_handler_13-proto_fault_stub_start
    .quad proto_handler_14-proto_fault_stub_start
    .quad proto_handler_15-proto_fault_stub_start
    .quad proto_handler_16-proto_fault_stub_start
    .quad proto_handler_17-proto_fault_stub_start
    .quad proto_handler_18-proto_fault_stub_start
    .quad proto_handler_19-proto_fault_stub_start
    .quad proto_handler_20-proto_fault_stub_start
    .quad proto_handler_21-proto_fault_stub_start

    .global proto_fault_handlers_count
    .type proto_fault_handlers_count, STT_OBJECT
proto_fault_handlers_count:
    .quad 21

    .global set_idtr
    .type set_idtr, STT_FUNC
set_idtr:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp          # allocate 16 bytes on the stack for the descriptor

    # Store the 16-bit limit (from the low 16 bits of RSI) at offset 0
    movw %si, (%rsp)

    # Store the 64-bit IDT base (from RDI) at offset 2.
    movq %rdi, 2(%rsp)

    # Load the IDTR using the descriptor pointed to by RSP.
    lidt (%rsp)

    addq $16, %rsp          # deallocate the temporary space
    popq %rbp
    ret
