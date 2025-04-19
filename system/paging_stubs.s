.section .text

    .global get_cr3
    .type get_cr3, STT_FUNC
get_cr3:
    # Returns current value of CR3
    mov %cr3, %rax
    ret

    .global set_cr3
    .type set_cr3, STT_FUNC
set_cr3:
    # Argument 1 (RDI) - new value of CR3
    mov %rdi, %cr3
    ret
