# arguments to this file:
# - ARCH
# - OUTDIR, the place to put object files
# - APPDIR, the place to put executable files
# - ACTION, build or clean

LIBDIRS :=  arch/$(ARCH) libc
LIBHDRS :=  $(shell find h -type f -name '*.h')
LIBSRCS :=  $(foreach d,$(LIBDIRS),$(shell find $(d) -type f -name '*.S' -o -name '*.c'))
LIBOBJS :=  $(patsubst %,$(OUTDIR)/%.o,$(LIBSRCS))

LIBC    :=  $(OUTDIR)/libc.a
APPS    :=  $(patsubst %/,%,$(dir $(wildcard */Makefile)))

CFLAGS  :=  -c -std=c17 -Ih -ffreestanding
CFLAGS  +=  -DSYSCALL_DEF='<../../common/syscall.def>'

include arch/$(ARCH)/config.mk

all: $(LIBC) $(APPS)

$(APPS):: $(LIBC)
	@ $(MAKE) -C $@ LIBC=$(LIBC) APPDIR=$(APPDIR) OUTDIR=$(OUTDIR)/$@ $(ACTION)

$(LIBC): $(LIBOBJS)
	@ mkdir -p $(@D) > /dev/null
	@ $(AR) rcs -o $@ $<

$(OUTDIR)/%.S.o: %.S
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(OUTDIR)/%.c.o: %.c $(LIBHDRS)
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<