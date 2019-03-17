DIRS    :=  setup
SRCS    :=  $(foreach d,$(DIRS),$(shell find $(d) -name '*.S' -o -name '*.c'))
OBJS    :=  $(patsubst %,$(OUTDIR)/%.o,$(SRCS))

CFLAGS  :=  -c -g -std=c99 -Ih -ffreestanding -fPIC -Wall -Wextra
CFLAGS  +=  -Werror=implicit -Werror=implicit-function-declaration

LFLAGS  :=  -nostdlib -lgcc -Ttext=0x100000

build: $(OUTDIR)/setup.app

clean:
	@ rm -rf $(OBJS)
	@ rm -rf $(OUTDIR)/setup.app

$(OUTDIR)/setup.app: $(OBJS)
	@ echo "[LD:U] $@"
	@ $(CC) $(LFLAGS) -o $@ $^

$(filter %.S.o, $(OBJS)): $(OUTDIR)/%.S.o: %.S
	@ echo "[AS:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<

$(filter %.c.o, $(OBJS)): $(OUTDIR)/%.c.o: %.c
	@ echo "[CC:U] $@"
	@ mkdir -p $(@D) > /dev/null
	@ $(CC) $(CFLAGS) -o $@ $<
