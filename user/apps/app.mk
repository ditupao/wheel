# template for building application programs
# user programs only need to include this file

ifeq ($(NAME),)
    $(error NAME not defined)
endif

SRCLIST ?=  $(patsubst ./%,%,$(shell find . -type f -name '*.S' -o -name '*.c'))
OBJLIST ?=  $(patsubst %,$(OUTDIR)/%.o,$(SRCLIST))
APPFILE ?=  $(APPDIR)/$(NAME).app
MAPFILE ?=  $(OUTDIR)/$(NAME).map

# TODO: different for each arch?
CFLAGS  :=  -c -g -std=c99 -I ../h -ffreestanding -fPIC
CFLAGS  +=  -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -mno-fma
CFLAGS  +=  -DSYSCALL_DEF='<../../common/syscall.def>'

LFLAGS  :=  -nostdlib -L $(OUTDIR)/.. -lc -lgcc -Ttext=0x100000

include ../arch/$(ARCH)/config.mk

build: $(APPFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(APPFILE)

$(APPFILE): $(OBJLIST) $(LIBC)
	@ echo "[LD:U] $@"
	@ mkdir -p $(@D) > /dev/null
	$(CC) $(LFLAGS) -Wl,-Map,$(MAPFILE) $^ -o $@

$(filter %.S.o, $(OBJLIST)): $(OUTDIR)/%.S.o: %.S
	@ echo "[AS:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(filter %.c.o, $(OBJLIST)): $(OUTDIR)/%.c.o: %.c
	@ echo "[CC:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<
