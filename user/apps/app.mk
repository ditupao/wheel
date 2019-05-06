# template for building application programs
# user programs only need to include this file

ifeq ($(NAME),)
    $(error NAME not defined)
endif

SRCLIST ?=  $(patsubst ./%,%,$(shell find . -type f -name '*.S' -o -name '*.c'))
OBJLIST ?=  $(patsubst %,$(OUTDIR)/%.o,$(SRCLIST))
APPFILE ?=  $(APPDIR)/$(NAME).app
MAPFILE ?=  $(OUTDIR)/$(NAME).map

CFLAGS  :=  -c -g -std=c99 -I../../libc/h -I../../../common -ffreestanding
CFLAGS  +=  -DSYSCALL_DEF='<syscall.def>'
LFLAGS  :=  -nostdlib -L $(APPDIR) -lc -lgcc

build: $(APPFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(APPFILE)

$(APPFILE): $(OBJLIST) $(LIBC)
	@ echo "[U:LD] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(LFLAGS) -Wl,-Map,$(MAPFILE) $^ -o $@

$(OUTDIR)/%.S.o: %.S
	@ echo "[U:AS] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(OUTDIR)/%.c.o: %.c
	@ echo "[U:CC] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<
