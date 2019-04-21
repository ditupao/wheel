# arguments to this file:
# - ARCH
# - OUTDIR, the place to put object files
# - APPDIR, the place to put executable files
# - ACTION, build or clean

SUBDIRS :=  $(patsubst %/,%,$(dir $(wildcard */Makefile)))

all: $(SUBDIRS)

$(SUBDIRS)::
	@ $(MAKE) -C $@ ARCH=$(ARCH) OUTDIR=$(OUTDIR)/$@ APPDIR=$(APPDIR) $(ACTION)
