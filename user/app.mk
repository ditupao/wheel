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
CFLAGS  :=  -c -g -std=c99 -ffreestanding -fPIC -Wall -Wextra
CFLAGS  +=  -Werror=implicit -Werror=implicit-function-declaration
LFLAGS  :=  -nostdlib -lgcc -Ttext=0x100000

build: $(APPFILE)

clean:
	@ rm -f $(OBJLIST)
	@ rm -f $(APPFILE)

$(APPFILE): $(OBJLIST)
	@ echo "[LD:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(LFLAGS) -Wl,-Map,$(MAPFILE) -o $@ $^

$(filter %.S.o, $(OBJLIST)): $(OUTDIR)/%.S.o: %.S
	@ echo "[AS:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(filter %.c.o, $(OBJLIST)): $(OUTDIR)/%.c.o: %.c
	@ echo "[CC:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<
