CC      :=  x86_64-elf-gcc
OBJCOPY :=  x86_64-elf-objcopy

CFLAGS  +=  -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -mno-fma
LFLAGS  +=  -z max-page-size=0x1000
