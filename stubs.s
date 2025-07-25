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

    .global write_msr_stub
    .type write_msr_stub, @function
write_msr_stub:
    # rdi - msr number
    # rsi - value to write
    movl %edi, %ecx
    mov %rsi, %rax
    mov %rsi, %rdx
    shr $32, %rdx
    wrmsr
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

    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro m_restore_regs
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

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
    rdtscp
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
    # GPF hit? -> 0xdead in %rax
    mov %rax, %r9

    rdtscp
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

    .global apply_ucode_execute_machine_code_simple
    .type apply_ucode_execute_machine_code_simple, @function
apply_ucode_execute_machine_code_simple:
    m_save_regs
    # backup the address of meta data into R8
    mov %rdi, %r8
    # set ucode update address from meta data
    movq 40(%rdi), %rdi
    m_apply_ucode
    # copy return values to metadata struct
    # r9 holds the value potentially overwritten by the GPF handler
    movq %r9, 56(%r8)
    movq %rax, 64(%r8)
    cmp $0xdead, %r9w
    je l_apply_ucode_execute_machine_code_simple_ret
    mov %r8, %rax
    movq 32(%r8), %r8
    call *%r8

l_apply_ucode_execute_machine_code_simple_ret:
    m_restore_regs
    ret

    .global apply_ucode_execute_machine_code_restore
    .type apply_ucode_execute_machine_code_restore, @function
apply_ucode_execute_machine_code_restore:
    m_save_regs
    # backup the address of meta data into R8
    mov %rdi, %r8
    # set ucode update address from meta data
    movq 40(%rdi), %rdi
    m_apply_ucode

    # copy return values to metadata struct
    # r9 holds the value potentially overwritten by the GPF handler
    movq %r9, 56(%r8)
    movq %rax, 64(%r8)
    cmp $0xdead, %r9w
    je l_apply_ucode_execute_machine_code_restore_ret
    mov %r8, %rax
    movq 32(%r8), %r8
    call *%r8

    lea original_ucode_s(%rip), %rdi
    m_apply_ucode
l_apply_ucode_execute_machine_code_restore_ret:
    m_restore_regs
    ret

    .global execute_machine_code
    .type execute_machine_code, @function
execute_machine_code:
    m_save_regs
    # write context pointer into RAX as defined by ABI
    mov %rdi, %rax
    # get machine code address to call
    movq 32(%rdi), %rdi
    call *%rdi

    m_restore_regs
    ret

    .global get_tsc
    .type get_tsc, @function
get_tsc:
    rdtsc
    shl $32, %rdx
    or %rdx, %rax
    ret

    .global interlocked_compare_exchange_64
    .type interlocked_compare_exchange_64, @function
interlocked_compare_exchange_64:
    #   RDI = mem
    #   RSI = compare_value
    #   RDX = exchange_value

    movq %rsi, %rax
    lock cmpxchgq %rdx, (%rdi)
    # (%rdi) == %rax -> (%rdi) = %rdx else %rax = (%rdi)
    # sucess -> get back compare_value, else get back current *mem
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

    .global call_cpuid
    .type call_cpuid, @function
call_cpuid:
    #   RDI = leaf/eax value
    push %rbx
    xor %rax, %rax
    mov %edi, %eax
    cpuid
    pop %rbx
    ret

    .global call_cpuid_ecx
    .type call_cpuid_ecx, @function
call_cpuid_ecx:
    #   RDI = leaf/eax value
    push %rbx
    xor %rax, %rax
    mov %edi, %eax
    cpuid
    pop %rbx
    mov %ecx, %eax
    ret

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
