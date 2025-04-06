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
    movq -0x10(%rsp), %rcx
    movq %rcx, 0x20(%rax)
    # original rbx
    movq -0x8(%rsp), %rcx
    movq %rcx, 0x28(%rax)
    # original rsp
    movq -0x18(%rsp), %rcx
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
    ret

get_core_ctx_ptr:
    movq proto_fault_stub_context_ptr(%rip), %rax
    ret


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

    # remove register backups from stack
    # our handlers do not return, so no need to preserve registers beyond this point
    add $24, %rsp

    # fault occured = 1
    movq $1, 0(%rax)

    # fault number
    movq $\handler_id, 0x8(%rax)

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

    # for now just endless loop to lock the core
    _loop_\@ :
    jmp _loop_\@
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

    # for now just endless loop to lock the core
    _loop_\@ :
    jmp _loop_\@
.endm

    .align 16
proto_handler_0:
    m_handler_no_error_code 0

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

    _loop_df_handler :
    jmp _loop_df_handler

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
    m_handler_with_error_code 13

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
