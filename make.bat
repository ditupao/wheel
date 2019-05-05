@ if "%*" == "run" (
    wsl make iso && qemu-system-x86_64 -smp 4 -m 256 -vga vmware ^
        -serial stdio -gdb tcp::4444 -cdrom bin\wheel.iso
) else (
    wsl make %*
)
