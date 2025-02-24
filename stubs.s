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

# Constant pool section
    .section .rodata
constant_pool:
    .quad 0x1111111111111111  # Unused constant 1
    .quad 0x2222222222222222  # Unused constant 2
    .quad 0xdeadbeefdeadc0de  # Actual constant used for XOR
