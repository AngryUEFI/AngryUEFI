.intel_syntax noprefix

    .global enable_dsm_in_cr4_stub
    .type enable_dsm_in_cr4_stub, @function
enable_dsm_in_cr4_stub:
    mov rax, cr4
    or rax, 0x8
    mov cr4, rax
    ret

    .align 64
    .global write_fenced_fptan
    .type write_fenced_fptan, @function
write_fenced_fptan:
    # rbx backup, SySV requires it to be preserved
    mov r8, rbx
    # need to adapt the System V calling convention to the interface expected by the ucode
    mov rax, rdi
    mov rbx, rsi
    mov rcx, rdx
    clts
    jmp .Lmcode_01_write_fenced_fptan
    .align 64
    .Lmcode_01_write_fenced_fptan:
    mfence
    fptan
    mfence
    jmp .Lmcode_02_write_fenced_fptan
    .align 64
    .Lmcode_02_write_fenced_fptan:
    # restore rbx backup
    mov rbx, r8
    ret
    .align 64

    .align 64
    .global read_fenced_fpatan
    .type read_fenced_fpatan, @function
read_fenced_fpatan:
    # rbx backup, SySV requires it to be preserved
    mov r8, rbx
    # need to adapt the System V calling convention to the interface expected by the ucode
    mov rax, rdi
    mov rbx, rsi
    mov rcx, rdx
    clts
    lfence
    jmp .Lmcode_01_read_fenced_fpatan
    .align 64
    .Lmcode_01_read_fenced_fpatan:
    mfence
    fpatan
    mfence
    jmp .Lmcode_02_read_fenced_fpatan
    .align 64
    .Lmcode_02_read_fenced_fpatan:
    lfence
    # restore rbx backup
    mov rbx, r8
    ret
    .align 64

    .align 64
    .global write_fenced_fprem
    .type write_fenced_fprem, @function
write_fenced_fprem:
    # rbx backup, SySV requires it to be preserved
    mov r8, rbx
    # need to adapt the System V calling convention to the interface expected by the ucode
    mov rax, rdi
    mov rbx, rsi
    mov rcx, rdx
    clts
    jmp .Lmcode_01_write_fenced_fprem
    .align 64
    .Lmcode_01_write_fenced_fprem:
    mfence
    fprem
    mfence
    jmp .Lmcode_02_write_fenced_fprem
    .align 64
    .Lmcode_02_write_fenced_fprem:
    # restore rbx backup
    mov rbx, r8
    ret
    .align 64

    .align 64
    .global write_fenced_fprem_asm
    .type write_fenced_fprem_asm, @function
write_fenced_fprem_asm:
    clts
    jmp .Lmcode_01_write_fenced_fprem_asm
    .align 64
    .Lmcode_01_write_fenced_fprem_asm:
    mfence
    fprem
    mfence
    jmp .Lmcode_02_write_fenced_fprem_asm
    .align 64
    .Lmcode_02_write_fenced_fprem_asm:
    ret
    .align 64
