    .section .text
    .global test_stub
    .type test_stub, @function

# Function: uint64_t test_stub(uint64_t a, uint64_t b)
# Arguments:
#   a -> RDI (1st argument in System V ABI)
#   b -> RSI (2nd argument)

test_stub:
    cli
    # Add the two parameters
    add %rsi, %rdi  # rdi = rdi + rsi

    # Load the constant from the constant pool
    lea constant_pool+16(%rip), %rax  # Load address of the constant into rax
    mov (%rax), %rax               # Load the actual constant value from memory into rax

    # XOR the result with the constant
    xor %rax, %rdi  # rdi = rdi XOR rax

    # Return the result in RAX
    mov %rdi, %rax  # Move result to rax (return register)
    sti
    ret             # Return from function

    .global read_msr_stub
    .type read_msr_stub, @function
read_msr_stub:
    movl %edi, %ecx
    xor %rdx, %rdx
    xor %rax, %rax
    rdmsr
    shl $32, %rdx
    or %rdx, %rax
    ret

.extern hook_gpf
.extern unhook_gpf

.macro m_save_regs
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
.endm

.macro m_restore_regs
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
.endm

# apply the ucode passed in RDI
# rdtsc(p) difference is stored in RAX
# use a macro instead of a function to reduce complex instructions like call/ret
.macro m_apply_ucode
    lea ucode_patch_address(%rip), %r10
    movl 0(%r10), %r10d
    # TODO: emulator does not support rdtscp
    rdtsc
    shl $32, %rdx
    or %rdx, %rax
    mov %rax, %r11

    # update ucode MSR
    mov %r10d, %ecx
    # update address, low part
    mov %edi, %eax
    mov %rdi, %rdx
    # update address, high part
    shr $32, %rdx
    wrmsr
    mov %rax, %r9

    rdtsc
    shl $32, %rdx
    or %rdx, %rax
    sub %r11, %rax
.endm

    .global apply_ucode_simple
    .type apply_ucode_simple, @function
apply_ucode_simple:
    m_apply_ucode
    # r9 holds the value potentially overwritten by the GPF handler
    movq %r9, (%rsi)
    ret

    .global apply_ucode_restore
    .type apply_ucode_restore, @function
apply_ucode_restore:
    m_apply_ucode
    # r9 holds the value potentially overwritten by the GPF handler
    movq %r9, (%rsi)

    mov %rax, %r9
    lea original_ucode_s(%rip), %rdi
    m_apply_ucode
    mov %r9, %rax
    ret

    .global read_idt_position
    .type read_idt_position, @function
read_idt_position:
    lea idt_structure_addr(%rip), %rax
    sidt (%rax)
    ret

    .global write_idt_position
    .type write_idt_position, @function
write_idt_position:
    lea idt_structure_addr(%rip), %rax
    lidt (%rax)
    ret

    .extern log_panic

    .global gpf_handler
    .type gpf_handler, @function
gpf_handler:
    mov 8(%rsp), %rax
    add $2, %rax
    mov %rax, 8(%rsp)
    mov $0xDEAD, %rax
    add $8, %rsp
    iretq

.extern original_ucode


    .section .data
    .align 8
gpf_backup:
    .space 8
idt_structure:
    .space 10

# Constant pool section
    .section .rodata
constant_pool:
    .quad 0x1111111111111111  # Unused constant 1
    .quad 0x2222222222222222  # Unused constant 2
    .quad 0xdeadbeefdeadc0de  # Actual constant used for XOR

    .align 8
original_ucode_s:
    .quad original_ucode
    .align 4
ucode_patch_address:
    .long 0xc0010020
    .align 8
idt_structure_addr:
    .quad idt_structure
gpf_backup_addr:
    .quad gpf_backup
