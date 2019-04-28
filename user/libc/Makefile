# makefile for building userland library

SUBDIRS :=  arch/$(ARCH) string
SRCLIST :=  $(foreach d,$(SUBDIRS),$(shell find $(d) -type f -name '*.S' -o -name '*.c'))
OBJLIST :=  $(patsubst %,$(OUTDIR)/%.o,$(SRCLIST))
DEPLIST :=  $(patsubst %.c.o,%.c.d,$(filter %.c.o,$(OBJLIST)))

LIBFILE :=  $(APPDIR)/libc.a

CFLAGS  :=  -c -g -std=c99 -I ../h -ffreestanding
CFLAGS  +=  -DSYSCALL_DEF='<../../common/syscall.def>'
DEPGEN   =  -MT $@ -MMD -MP -MF $(basename $@).t

include arch/$(ARCH)/config.mk

build: $(LIBFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(LIBFILE)

$(LIBFILE): $(OBJLIST)
	@ echo "[AR:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(AR) rcs -o $@ $^

# $(filter %.S.o, $(OBJLIST)): $(OUTDIR)/%.S.o: %.S
$(OUTDIR)/%.S.o: %.S
	@ echo "[AS:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

# $(filter %.c.o, $(OBJLIST)): $(OUTDIR)/%.c.o: %.c $(OUTDIR)/%.c.d
$(OUTDIR)/%.c.o: %.c $(OUTDIR)/%.c.d
	@ echo "[CC:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) $(DEPGEN) -o $@ $<
	@ mv -f $(basename $@).t $(basename $@).d

$(DEPLIST): ;
.PRECIOUS: $(DEPLIST)

-include $(DEPLIST)
