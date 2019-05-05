# arguments to this file:
# - ARCH
# - APPDIR, the place to put executable files
# - OUTDIR, the place to put object files
# - ACTION, build or clean

TOOLS   :=  ARCH=$(ARCH) CC=$(CC) AR=$(AR) OBJCOPY=$(OBJCOPY)

build:
	@ $(MAKE) -C libc $(TOOLS) APPDIR=$(APPDIR) OUTDIR=$(OUTDIR)/libc        build
	@ $(MAKE) -C apps $(TOOLS) APPDIR=$(APPDIR) OUTDIR=$(OUTDIR)/apps ACTION=build

clean:
	@ $(MAKE) -C libc $(TOOLS) APPDIR=$(APPDIR) OUTDIR=$(OUTDIR)/libc        clean
	@ $(MAKE) -C apps $(TOOLS) APPDIR=$(APPDIR) OUTDIR=$(OUTDIR)/apps ACTION=clean