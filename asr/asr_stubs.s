.intel_syntax noprefix

    .extern get_asr_for_index
    .global call_asr_for_index
    .type call_asr_for_index, @function
call_asr_for_index:
    # save volatile registers, this is a C function
    push rdi
    push rsi
    push rdx
    push rcx
    push r8
    push r9
    # rdi is already context ptr
    # rsi = index
    mov rsi, rax
    call get_asr_for_index
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    # rax == NULL -> not found
    cmp rax, 0
    jnz call_func
    ret
call_func:
    # registers are already setup correctly
    # stack is also as on entry in case stack args are passed
    # tail call
    jmp rax
