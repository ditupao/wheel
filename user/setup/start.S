// TODO: move this into crt
// create a static library for linking with user programs

.global _start
.extern _entry

.text
.code64

_start:
    // sections are loaded at correct location
    // stack space are allocated by the kernel
    call    _entry
    ret