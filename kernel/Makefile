# makefile for wheel kernel

TARFILE :=  $(OUTDIR)/ramfs.tar
RAMFILE :=  $(OUTDIR)/ramfs.o

ELFFILE :=  $(OUTDIR)/wheel.elf
BINFILE :=  $(OUTDIR)/wheel.bin
MAPFILE :=  $(OUTDIR)/wheel.map
SYMFILE :=  $(OUTDIR)/wheel.sym

SUBDIRS :=  arch/$(ARCH) core mem drvs libk
SRCLIST :=  $(foreach d,$(SUBDIRS),$(shell find $(d) -type f -name '*.S' -o -name '*.c'))
OBJLIST :=  $(patsubst %,$(OUTDIR)/%.o,$(SRCLIST))
DEPLIST :=  $(patsubst %.c.o,%.c.d,$(filter %.c.o,$(OBJLIST)))
APPLIST :=  $(shell find $(APPDIR) -type f -name '*.app')

CFLAGS  :=  -c -g -std=c17 -Ih -Ih/arch/$(ARCH) -I../common -ffreestanding
CFLAGS  +=  -Wall -Wextra -Werror=implicit -Wno-cast-function-type
CFLAGS  +=  -DSYSCALL_DEF='<syscall.def>'
DEPGEN   =  -MT $@ -MMD -MP -MF $(basename $@).t
LFLAGS  :=  -nostdlib -lgcc -T arch/$(ARCH)/layout.ld -Wl,-Map,$(MAPFILE)

include arch/$(ARCH)/config.mk

build:  $(BINFILE) $(SYMFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(DEPLIST)
	@ rm -f $(TARFILE)
	@ rm -f $(RAMFILE)
	@ rm -f $(ELFFILE)
	@ rm -f $(BINFILE)
	@ rm -f $(MAPFILE)
	@ rm -f $(SYMFILE)

$(RAMFILE): $(TARFILE)
	@ echo "[K:CP] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture=i386 --rename-section .data=.binary $< $@

$(TARFILE): $(APPLIST)
	@ mkdir -p $(@D) > /dev/null
	@ tar -cf $@ -C $(APPDIR) $(patsubst $(APPDIR)/%,%,$^)

$(ELFFILE): $(OBJLIST) $(RAMFILE)
	@ echo "[K:LD] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(LFLAGS) -o $@ $^

$(BINFILE): $(ELFFILE)
	@ echo "[K:CP] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(OBJCOPY) --strip-debug $< $@

$(SYMFILE): $(ELFFILE)
	@ echo "[K:CP] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(OBJCOPY) --only-keep-debug $< $@

$(OUTDIR)/%.S.o: %.S
	@ echo "[K:AS] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(OUTDIR)/%.c.o: %.c $(OUTDIR)/%.c.d
	@ echo "[K:CC] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) $(DEPGEN) -o $@ $<
	@ mv -f $(basename $@).t $(basename $@).d

$(DEPLIST): ;
.PRECIOUS: $(DEPLIST)

-include $(DEPLIST)
