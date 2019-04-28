.global _start

.extern entry
.extern exit

.text
.code64

// this is the real entry of user program
_start:
    movq    %rsp, %rdi          // points to argv[0]

    pushq   $0                  // dummy return code
    pushq   $0                  // dummy rbp value
    movq    %rsp, %rbp

    call    entry

    movl    %eax, %edi          // return value of main()
    call    exit                // should never return
1:
    jmp     1b
