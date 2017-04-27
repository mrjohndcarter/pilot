# Makefile for Pilot library as of V3.1
#
# make [all]	build library with both C and Fortran APIs
# make cpilot	build only C API
#
# make install	copy library to $PREFIX/lib and user header files to
#		$PREFIX/include, creating those directories if they do not
#		already exist.
# make install-man  copy man pages to $DOC_PREFIX/man, creating those directories
#		if they do not already exist, and "invoking docs/make dist-docs"
#		if man pages were not yet generated
#
# NOTE: depending on where $PREFIX and $DOC_PREFIX are, you may need to run
# "make install..." the first time to generate the files, and
# "sudo make install..." a second time to copy them over.
#
# make clean	cleans trunk only
# make clean-all  also cleans docs and tests subdirs
#
# [4-Oct-09] Adding Fortran API files and separate cpilot target.
# [12-May-14] Changing from .f90 to .F90 to get auto invocation of preprocessor
# [15-May-14] Make flag settings dependent on compiler vendor
# [22-May-14] Create installation info banner for pilot.h
# [29-Jul-16] Add optional MPE library for Jumpshot viewable log (-pisvc=j)

# Change to your local Pilot installation by editing the next line(s) or by
# overriding like so:
#			make install PREFIX=/opt/pilot

PREFIX = /usr/local
DOC_PREFIX = $(PREFIX)

# If you have installed the MPE library and want to use it, uncomment and
# adjust this line, otherwise, -pisvc=j won't be available.  Note, this only
# gets Pilot compiled with MPE.  You also have to link your Pilot application
# with it in the app's makefile or you will get undefined refs to MPE_...
#
#MPEHOME = /usr

# This should be some (re)writeable file system
#
TMPFILE = /tmp/pilot-installation-info

# Compiling C, CFLAGS:
#  If you only want the C API, and your MPI installation is causing problems
#  compiling Fortran datatypes that pilot.c wants by default, then add
#  -DMPID_NO_FORTRAN to CFLAGS, and "make cpilot".  If by mistake you try to
#  build the Fortran API ("make") with -DMPID_NO_FORTRAN, you will get
#  "Format is invalid" errors at run time when you use it.
#
#  HP, gets lots of "type-punning" warnings w/o -fno-strict-aliasing
#  Sun, -v
#  gfort, -Wtabs # suppress warnings about nonconforming tabs
#  Intel, -wd869 # suppress remark #869 "parameter never referenced"
#
# Set your C and Fortran 90 compilers here...

CC = mpicc
FC = mpif90

# Probe for the vendor and set flags accordingly
#	They all seem to respond to --version
#	Some print to stderr, so use "2>&1" to combine with stdout
#	grep is because some may emit leading blank lines
#	On the first line, take the first blank-delimited symbol

VENDOR = $(shell $(CC) --version 2>&1 | grep . | head -1 | cut -d' ' -f1)

ifeq "$(VENDOR)" "icc"		# Intel compilers
  CFLAGS = -Wall -O2 -wd869
  FFLAGS = -O2
endif
ifeq "$(VENDOR)" "gcc"		# GNU compilers
  CFLAGS = -Wall -O2
  FFLAGS = -Wall -O2 -Wtabs
endif
ifeq "$(VENDOR)" "Open64"	# CAPSL compilers
  CFLAGS = -Wall -O2
  FFLAGS =
endif
ifeq "$(VENDOR)" "pgcc"		# PGI compilers
  CFLAGS = -fast
  FFLAGS = -fast
endif
ifeq "$(VENDOR)" "Pathscale"	# Pathscale compilers
  CFLAGS = -Wall -O2
  FFLAGS = -Wall -O2 
endif

# Some mpirun only like -version, not --version
#
CC_INFO = "$(shell $(CC) --version 2>&1)"
MPI_INFO = "$(shell mpirun -version)"	# doesn't like to run as root

# If MPE is available, then turn MPE logging code on
#
ifdef MPEHOME
  CFLAGS += -DPILOT_WITH_MPE -I$(MPEHOME)/include
endif

all:	cpilot fpilot

cpilot: libpilot.a banner

libpilot.a: pilot.o pilot_deadlock.o
	ar rs libpilot.a pilot.o pilot_deadlock.o

banner: pilot.h
	# create a temp file of pilot.h with installation info banner
	echo \
	  "/************************* PILOT INSTALLATION ******************************" \
	  > $(TMPFILE)
	date >> $(TMPFILE)
	uname -a >> $(TMPFILE)		# platform info
	echo $(MPI_INFO) >> $(TMPFILE)
	echo $(CC_INFO) >> $(TMPFILE)
	echo \
	  "***************************************************************************/" \
	  >> $(TMPFILE)
	cat pilot.h >> $(TMPFILE)
	
fpilot:	libpilot.a fpilot_private.o
	ar rs libpilot.a fpilot_private.o	

pilot_private.h: pilot_limits.h

pilot_deadlock.h: pilot.h pilot_private.h

pilot.o: pilot.c pilot.h pilot_error.h pilot_private.h pilot_deadlock.h
	$(CC) $(CFLAGS) -c $< -o $@

pilot_deadlock.o: pilot_deadlock.c pilot_deadlock.h
	$(CC) $(CFLAGS) -c $< -o $@

fpilot_private.o: fpilot_private.F90 pilot.for pilot_limits.h
	$(FC) $(FFLAGS) -c $< -o $@

install: libpilot.a
	# copy .mod and .for if Fortran API was built
	mkdir -p $(PREFIX)/include/ && \
	cp pilot.h pilot_limits.h $(PREFIX)/include/ && \
	if [ -f fpilot_private.mod ]; \
	  then cp fpilot_private.mod pilot.for $(PREFIX)/include/; fi
	mkdir -p $(PREFIX)/lib/ && \
	cp libpilot.a $(PREFIX)/lib/
	# if bannerized pilot.h is available, copy that instead
	if [ -f $(TMPFILE) ]; \
	  then cp $(TMPFILE) $(PREFIX)/include/pilot.h; fi
	
docs/man/man3:
	$(MAKE) -C docs dist-docs

install-man: docs/man/man3
	mkdir -p $(DOC_PREFIX) && \
	cp -r docs/man $(DOC_PREFIX)

clean:
	rm -f *.a *.o *.mod

clean-all: clean
	$(MAKE) -C docs clean
	$(MAKE) -C docs/code_examples clean
	$(MAKE) -C tests clean
