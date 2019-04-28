# later we should remove all these flags
# default options should work for user programs

CFLAGS  +=  -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -mno-fma
LFLAGS  +=  -z max-page-size=0x1000
