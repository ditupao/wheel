# makefile for building userland library

SUBDIRS :=  arch/$(ARCH)
SRCLIST :=  $(foreach d,$(SUBDIRS),$(shell find $(d) -type f -name '*.S' -o -name '*.c'))
OBJLIST :=  $(patsubst %,$(OUTDIR)/%.o,$(SRCLIST))
DEPLIST :=  $(patsubst %.c.o,%.c.d,$(filter %.c.o,$(OBJLIST)))

LIBFILE :=  $(APPDIR)/libc.a

CFLAGS  :=  -c -g -std=c99 -Ih -I../../common -ffreestanding
CFLAGS  +=  -DSYSCALL_DEF='<syscall.def>'
DEPGEN   =  -MT $@ -MMD -MP -MF $(basename $@).t

# include arch/$(ARCH)/config.mk

build: $(LIBFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(LIBFILE)

$(LIBFILE): $(OBJLIST)
	@ echo "[U:AR] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(AR) rcs -o $@ $^

$(OUTDIR)/%.S.o: %.S
	@ echo "[U:AS] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(OUTDIR)/%.c.o: %.c $(OUTDIR)/%.c.d
	@ echo "[U:CC] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) $(DEPGEN) -o $@ $<
	@ mv -f $(basename $@).t $(basename $@).d

$(DEPLIST): ;
.PRECIOUS: $(DEPLIST)

-include $(DEPLIST)
