# top level makefile for wheel operating system

OUTDIR  :=  $(CURDIR)/bin
ISODIR  :=  $(OUTDIR)/iso

TGTARCH :=  x86_64
BINFILE :=  $(OUTDIR)/kernel/wheel.bin  # must be same with kernel/Makefile
ISOFILE :=  $(OUTDIR)/wheel.iso

build:
	@ echo "building the wheel operating system"
	@ $(MAKE) -C user   ARCH=$(TGTARCH) APPDIR=$(OUTDIR)/apps OUTDIR=$(OUTDIR)/user   ACTION=build
	@ $(MAKE) -C kernel ARCH=$(TGTARCH) APPDIR=$(OUTDIR)/apps OUTDIR=$(OUTDIR)/kernel        build

clean:
	@ echo "cleaning the wheel operating system"
	@ $(MAKE) -C user   ARCH=$(TGTARCH) APPDIR=$(OUTDIR)/apps OUTDIR=$(OUTDIR)/user   ACTION=clean
	@ $(MAKE) -C kernel ARCH=$(TGTARCH) APPDIR=$(OUTDIR)/apps OUTDIR=$(OUTDIR)/kernel        clean
	@ rm -rf $(ISOFILE)

iso: build
	@ rm -rf $(ISODIR)
	@ mkdir -p $(ISODIR)/boot/grub
	@ touch $(ISODIR)/boot/grub/grub.cfg
	@ echo "set default=0"            >> $(ISODIR)/boot/grub/grub.cfg
	@ echo "menuentry \"wheel\" {"    >> $(ISODIR)/boot/grub/grub.cfg
	@ echo "    multiboot /wheel.bin" >> $(ISODIR)/boot/grub/grub.cfg
	@ echo "    boot"                 >> $(ISODIR)/boot/grub/grub.cfg
	@ echo "}"                        >> $(ISODIR)/boot/grub/grub.cfg
	@ cp $(BINFILE) $(ISODIR)/wheel.bin
	@ grub-mkrescue -o $(ISOFILE) $(ISODIR) 2> /dev/null

run: iso
	@ qemu-system-x86_64 -smp 4 -m 256 -vga vmware -serial stdio -gdb tcp::4444 -cdrom $(ISOFILE)

loc:
	@ find . -type f -name "*.S" -o -name "*.c" -o -name "*.h" | xargs wc -l
