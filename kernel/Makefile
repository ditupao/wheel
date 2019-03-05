# makefile for wheel kernel

ELFFILE :=  $(OUTDIR)/wheel.elf
BINFILE :=  $(OUTDIR)/wheel.bin
MAPFILE :=  $(OUTDIR)/wheel.map
SYMFILE :=  $(OUTDIR)/wheel.sym

DIRS    :=  arch/$(ARCH) core libk
SRCS    :=  $(foreach d,$(DIRS),$(shell find $(d) -name '*.S' -o -name '*.c'))
OBJS    :=  $(patsubst %,$(OUTDIR)/%.o,$(SRCS))
DEPS    :=  $(patsubst %.c.o,%.c.d,$(filter %.c.o,$(OBJS)))

CFLAGS  :=  -c -g -std=c99 -Ih -Ih/arch/$(ARCH) -ffreestanding -Wall -Wextra
CFLAGS  +=  -Werror=implicit -Werror=implicit-function-declaration
DEPGEN   =  -MT $@ -MMD -MP -MF $(basename $@).t
LFLAGS  :=  -nostdlib -lgcc -T arch/$(ARCH)/layout.ld -Wl,-Map,$(MAPFILE)

include arch/$(ARCH)/config.mk

build:  $(BINFILE) $(SYMFILE)

clean:
	@ $(RM) $(ELFFILE)
	@ $(RM) $(BINFILE)
	@ $(RM) $(MAPFILE)
	@ $(RM) $(SYMFILE)
	@ $(RM) $(OBJS)
	@ $(RM) $(DEPS)

ifdef RAMFS
RAMOBJ := $(OUTDIR)/ramfs.o
$(RAMOBJ): $(RAMFS)
	@ echo "[K:CP] $@"
	@ $(OBJCOPY) --input binary --output elf64-x86-64 --binary-architecture=i386 --rename-section .data=.binary $< $@
endif

$(ELFFILE): $(OBJS) $(RAMOBJ)
	@ echo "[K:LD] $@"
	@ $(CC) $(LFLAGS) -o $@ $^

$(BINFILE): $(ELFFILE)
	@ echo "[K:CP] $@"
	@ $(OBJCOPY) --strip-debug $< $@

$(SYMFILE): $(ELFFILE)
	@ echo "[K:CP] $@"
	@ $(OBJCOPY) --only-keep-debug $< $@

$(filter %.S.o, $(OBJS)): $(OUTDIR)/%.S.o: %.S
	@ echo "[K:AS] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(filter %.c.o, $(OBJS)): $(OUTDIR)/%.c.o: %.c $(OUTDIR)/%.c.d
	@ echo "[K:CC] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) $(DEPGEN) -o $@ $<
	@ mv -f $(basename $@).t $(basename $@).d

$(DEPS): ;
.PRECIOUS: $(DEPS)

-include $(DEPS)
