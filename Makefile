# top level makefile for wheel operating system

OUTDIR  :=  $(CURDIR)/bin
ISODIR  :=  $(OUTDIR)/iso

TGTARCH :=  x86_64
APPFILE :=  $(OUTDIR)/user/setup.app    # must be same with user/Makefile
BINFILE :=  $(OUTDIR)/kernel/wheel.bin  # must be same with kernel/Makefile
ISOFILE :=  $(OUTDIR)/wheel.iso

build:
	@ echo "building the wheel operating system"
	@ $(MAKE) -C user   ARCH=$(TGTARCH) OUTDIR=$(OUTDIR)/user   APPDIR=$(OUTDIR)/apps ACTION=build
	@ $(MAKE) -C kernel ARCH=$(TGTARCH) OUTDIR=$(OUTDIR)/kernel APPDIR=$(OUTDIR)/apps        build

clean:
	@ echo "cleaning the wheel operating system"
	@ $(MAKE) -C user   ARCH=$(TGTARCH) OUTDIR=$(OUTDIR)/user   APPDIR=$(OUTDIR)/apps ACTION=clean
	@ $(MAKE) -C kernel ARCH=$(TGTARCH) OUTDIR=$(OUTDIR)/kernel APPDIR=$(OUTDIR)/apps        clean
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
