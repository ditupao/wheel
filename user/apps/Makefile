# arguments to this file:
# - ARCH
# - OUTDIR, the place to put object files
# - APPDIR, the place to put executable files
# - ACTION, build or clean

SUBDIRS :=  $(patsubst %/,%,$(dir $(wildcard */Makefile)))

PARAMS  :=  ARCH=$(ARCH) APPDIR=$(APPDIR) \
            CC=$(CC) AR=$(AR) OBJCOPY=$(OBJCOPY)

all: $(SUBDIRS)

$(SUBDIRS)::
	@ $(MAKE) -C $@ $(PARAMS) OUTDIR=$(OUTDIR)/$@ $(ACTION)
