wheel operating system
======================

Wheel is an operating system, written mostly from scratch. Currently we support 64-bit Intel/AMD architecture.

Some features of wheel:

- kernel mode multi-tasking
- fixed-priority preemptive scheduling
- support for symmetric multiprocessing (SMP)

To build and run wheel:

- `make` to generate kernel image `bin/wheel.bin`.
- `make iso` to create ISO image `bin/wheel.iso`.
- `make run` to run the OS with QEMU.
- `make clean` to delete all generated files

Required tools and softwares:

- Linux or WSL (Windows Subsystem for Linux).
- GCC cross compiler for x86_64 target, with names like `x86_64-elf-***`.
- GRUB, xorriso and mtools to make bootable iso image (also install `grub-pc-bin` on EFI systems).
- QEMU (or other virtual machine) to run the system.

References:

- [GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler)
- [Libgcc without red zone](https://wiki.osdev.org/Libgcc_without_red_zone)
