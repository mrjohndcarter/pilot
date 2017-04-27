# Makefile for Pilot unit tests
# [02-Jun-09] Converted to link with libpilot.a (EM)
# [13-Aug-09] Clean deadlock tests' .job files (BG)
# [ 9-Jan-10] libcheck target doesn't need Fortran API; make cpilot (BG)
# [28-May-11] Add tests for V2.0 (BG)
# [16-Sep-13] Add tests for V2.1 and simplify Makefile (BG)
# [29-Jul-16] Add flags for MPE library in V3.1 (BG)
# [ 3-Feb-17] Add demo_log program with V3.1 (BG)
# [ 8-Feb-17] Added FORTRAN version fdemo_log with V3.2 (BG)

# make [all]	build regression tests suite (needs CUnit) and demo_log
#		See 'run.sh' to run test suite
#       demo_log needs 9 MPI processes to run; use -pisvc=j
#
# make fdemo_log    build Fortran version of log demo
#       fdemo_log needs 9 MPI processes to run; use -pisvc=j
#
# make dl	build deadlock tests
#		See 'deadlock_tests[_qsub].sh' to run

# This uses the Pilot library assumed to be in ".." one level up, and will
# issue "make cpilot" there if necessary (or just "make" when building fdemo_log to
# get the FORTRAN API).
#
PILOTHOME = ..

# SHARCNET clusters
CUNITHOME = /work/wgardner

# carmel
#CUNITHOME = /usr

MPI_CC = mpicc
CFLAGS += -I$(PILOTHOME) -I$(CUNITHOME)/include
LDFLAGS += -L$(PILOTHOME) -lpilot -L$(CUNITHOME)/lib -lcunit -Wl,-rpath,$(CUNITHOME)/lib

MPI_FC = mpif90
FFLAGS += -I$(PILOTHOME)
# LDFLAGS same as for C

# ===========MPE library============
# We don't normally run the unit tests using -pisvc=j (MPE logging), but if Pilot was compiled
# with MPE, we still have to link with it, so uncomment/modify these lines.
#
# You CAN run with -pisvc=j.  Since each suite keeps reinitializing Pilot, only the last
# suite's log will remain in the .clog2 file, but you can view it with Jumpshot.
#
# demo_log (and fdemo_log) are intended for use with MPE logging and show all Pilot API calls
# in Jumpshot.
#
#MPEHOME = /scratch/wgardner/mpe
#LDFLAGS += -L$(MPEHOME)/lib -lmpe

all: libcheck test_suite demo_log

test_suite: unittests_main.o single_rw_suite.o array_rw_suite.o \
	mixed_value_suite.o selector_suite.o broadcaster_suite.o \
	gatherer_suite.o scatterer_suite.o  \
	extra_read_write_suite.o format_suite.o \
	init_suite.o config_suite.o reducer_suite.o
	$(MPI_CC) $^ $(LDFLAGS) -o $@

demo_log: demo_log.o
	$(MPI_CC) $^ $(LDFLAGS) -o $@
    
fdemo_log: fdemo_log.o flibcheck
	$(MPI_FC) $< $(LDFLAGS) -o $@

dl: deadlock/test_dead_wait.case \
	deadlock/test_dead_wait_select.case \
	deadlock/test_late_dead_wait_read.case \
	deadlock/test_late_dead_wait_write.case \
	deadlock/test_deadly_embrace.case \
	deadlock/test_unsatisfiable_select.case \
	deadlock/three_proc_cycle_read.case \
	deadlock/three_proc_cycle_write.case \
	deadlock/three_proc_cycle_select.case \
	deadlock/four_proc_cycle_read.case \
	deadlock/test_dead_wait_broadcast.case \
	deadlock/three_proc_cycle_gather.case \
	deadlock/scatter_deadly_embrace.case \
	deadlock/test_dead_wait_reduce.case

libcheck:
	@cd .. && $(MAKE) cpilot MPEHOME=$(MPEHOME)

flibcheck:
	@cd .. && $(MAKE) MPEHOME=$(MPEHOME)    # also builds Fortran API

clean:
	$(RM) *.o *.mod
	$(RM) test_suite demo_log fdemo_log
	$(RM) *.job* deadlock/*.case deadlock/*.o

%.o: %.c
	$(MPI_CC) $(CFLAGS) -c $< -o $@

%.o: %.F90
	$(MPI_FC) $(FFLAGS) -c $< -o $@

%.case: %.o
	$(MPI_CC) $< $(LDFLAGS) -o $@
