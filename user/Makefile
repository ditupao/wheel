# arguments to this file:
# - ARCH
# - APPDIR, the place to put executable files
# - OUTDIR, the place to put object files
# - ACTION, build or clean

PARAMS  :=  ARCH=$(ARCH) APPDIR=$(APPDIR) \
            CC=$(CC) AR=$(AR) OBJCOPY=$(OBJCOPY)

build:
	@ $(MAKE) -C libc $(PARAMS) OUTDIR=$(OUTDIR)/libc        build
	@ $(MAKE) -C apps $(PARAMS) OUTDIR=$(OUTDIR)/apps ACTION=build

clean:
	@ $(MAKE) -C libc $(PARAMS) OUTDIR=$(OUTDIR)/libc        clean
	@ $(MAKE) -C apps $(PARAMS) OUTDIR=$(OUTDIR)/apps ACTION=clean
