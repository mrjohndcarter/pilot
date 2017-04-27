# Makefile for Pilot code samples
#
# These can all be run using 3 MPI processes.
#
# If you installed Pilot with MPE, you should comment out these next lines and modify MPEHOME.
# Then you can enable -pisvc=j on the command line when you run each program, and view the
# resulting .clog2 logfile using Jumpshot.
#
# MPEHOME = /path/to/mpe
# LDFLAGS += -L$(MPEHOME)/lib -lmpe

all: ex_hello.sample ex_rw.sample ex_one_func.sample ex_array.sample ex_bundle.sample ex_collective.sample

%.sample: %.o
	mpicc $< -L../.. -lpilot -o $@

.c.o:
	mpicc $(CFLAGS) -I../.. -c $< -o $@

clean:
	$(RM) *.o *.sample
