#!/bin/bash
# This is the test runner for the deadlock unit tests.  This version is adapted
# for clusters that can use 'sqsub' to start batch jobs.  It starts all the jobs
# and then checks their output.  It may be useful on clusters where 'mpirun'
# starts all MPI processes on the login node, instead of dispersing them to
# separate nodes, since the latter is the normal parallel use case and will
# produce different execution orders.
#
# This submission procedure works on SHARCNET's mako cluster.

NPROCS=5	# no. of MPI/Pilot processes needed
LIMIT=2		# time limit per job in minutes

##################################################################
# Here are all the possible deadlock reasons in pilot_deadlock.c
#
REASON_DE="Conflicting channels create deadly embrace"
REASON_CW="Operation creates circular wait with above processes"
REASON_OX="Process at other end of channel has exited"
REASON_XH="Process exiting leaves earlier operation hung"
REASON_SN="Select cannot be fulfilled"
REASON_ES="Earlier select cannot be fulfilled"
##################################################################

find_text() {
    # $1 - text to search within
    # $2 - text to search for
    # $3 - alternate text to search for or ""
    [[ "$3" ]] && [[ "$1" =~ "$3" ]] || [[ "$1" =~ "$2" ]]
}

start_test() {
    # $1 - name of test executable
    printf "  $1... "

    # submit the job after removing any old output file
    rm -f $1.job.out
    tmp=$(sqsub -q mpi -r 2 -n $NPROCS -o $1.job.out deadlock/$1.case -pisvc=d 2>&1)
    if find_text "$tmp" "submitted as"; then
        echo "submitted"
    else
        echo "FAILURE"
        echo "$tmp"
	exit
    fi
}

check_test() {
    # $1 - name of test executable
    # $2 - expected error code
    # $3 - optional alternate error code
    printf "  $1... "

    # wait for job output file to appear
    #  NOTE: On SHARCNET file system, even after file is there, "test -s" may not find it
    #  until some passage of time
    until test -s $1.job.out; do echo -n "*"; sleep 1; done

    tmp=$(<$1.job.o*)
    if find_text "$tmp" "$2" "$3"; then
        echo "success"
    else
        echo "FAILURE"
        echo "$tmp"
    fi
}

# cleanup any previous jobs & output
rm *.job*

starttime=`date`
echo "Started $starttime"
echo "Submitting test jobs, may pause until job accepted..."

start_test "test_dead_wait"
start_test "test_dead_wait_select"
start_test "test_dead_wait_broadcast"
start_test "test_late_dead_wait_read"
start_test "test_late_dead_wait_write"
start_test "test_deadly_embrace"
start_test "three_proc_cycle_read"
start_test "three_proc_cycle_write"
start_test "three_proc_cycle_select"
start_test "three_proc_cycle_gather"
start_test "four_proc_cycle_read"
start_test "test_unsatisfiable_select"
start_test "scatter_deadly_embrace"
start_test "test_dead_wait_reduce"

echo "Checking output, may pause until job completes..."

check_test "test_dead_wait"		"$REASON_OX" "$REASON_XH"
check_test "test_dead_wait_select"	"$REASON_SN" "$REASON_XH"
check_test "test_dead_wait_broadcast"	"$REASON_OX" "$REASON_XH"
check_test "test_late_dead_wait_read"	"$REASON_OX" "$REASON_XH"
check_test "test_late_dead_wait_write"	"$REASON_OX" "$REASON_XH"
check_test "test_deadly_embrace"	"$REASON_DE"
check_test "three_proc_cycle_read"	"$REASON_CW"
check_test "three_proc_cycle_write"	"$REASON_CW"
check_test "three_proc_cycle_select"	"$REASON_CW" "$REASON_SN"
check_test "three_proc_cycle_gather"	"$REASON_CW"
check_test "four_proc_cycle_read"	"$REASON_CW"
check_test "test_unsatisfiable_select"	"$REASON_SN" "$REASON_ES"
check_test "scatter_deadly_embrace"	"$REASON_DE"
check_test "test_dead_wait_reduce"	"$REASON_OX" "$REASON_XH"


endtime=`date`
echo "Run ended on $endtime"
