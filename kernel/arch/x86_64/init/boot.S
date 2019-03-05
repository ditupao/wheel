#define MB_MAGIC    0x1badb002
#define MB_FLAGS    1<<0 | 1<<1
#define MB_CHECK    (- (MB_MAGIC + MB_FLAGS))

.global sys_boot

.section .boot, "ax"
.code32

load:
    jmp     sys_boot

.balign 4, 0
mb_header:
    .long   MB_MAGIC
    .long   MB_FLAGS
    .long   MB_CHECK

.balign 8, 0
sys_boot:
    movb    $'B', %al
    movb    $0x4e, %ah
    movw    %ax, (0xb8000)

1:
    jmp     1b
