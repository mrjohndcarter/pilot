/*
Entry point of the test suite. If you wish to add a new suite, simply add the
suite's registration function to the array of SuiteRegisterFuncs below.

See "unittests.h" for the def'n of SuiteRegisterFunc.
*/

/*
The number of required processes for the tests to run successfully. This number
is the maximum number of processes required by all test suites plus one process
for PI_MAIN.

That is, NUM_REQUIRED_PROCS should be:

    max(process_count(s) for all s in suites) + 1

If your suite uses more than the number below, you will have to increase this
number.
*/
#define NUM_REQUIRED_PROCS 8

#include "unittests.h"
#include <stdio.h>

int default_argc;
char** default_argv;
int my_rank;

// Add new suite registration functions to this list.
static SuiteRegisterFunc suites[] = {
    AddInitSuite,
    AddSingleRWSuite,
    AddArrayRWSuite,
    AddMixedValueSuite,
    AddSelectorSuite,
    AddBroadcasterSuite,
    AddGathererSuite,
    AddExtraReadWriteSuite,
    AddFormatSuite,
    AddConfigSuite,
    AddScattererSuite,
    AddReducerSuite,

    NULL,
};

int main(int argc, char* argv[])
{
    int numProcs;
    int i;
    CU_ErrorCode err = CUE_SUCCESS;

    // Calling MPI_Init will put Pilot into "Bench Mode".
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
    
    // Each suite's init() function calls PI_Configure with these defaults
    //  (which are the real ones after MPI_init removed any of its args), and
    //  Level 3 error checking needs the real argv to locate the bottom of the
    //  stack.  But in principle, a suite could feed different args to PI_Configure
    //  if it wants.
    default_argc = argc;
    default_argv = argv;

    if (numProcs < NUM_REQUIRED_PROCS) {
        if (my_rank == 0) {
            fprintf(stderr,
                "Error:\n"
                "  There are not enough MPI processes available to run the unit tests.\n"
                "  Please specifiy a number >= %d.\n",
                NUM_REQUIRED_PROCS);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // Only Rank 0 will print test results; others will be silent
    CU_BasicRunMode runMode =
            my_rank == 0 ? CU_BRM_VERBOSE : CU_BRM_SILENT;

    if (CU_initialize_registry() != CUE_SUCCESS)
        return CU_get_error();

    // Add all the suites
    for (i = 0; suites[i] != NULL; i++) {
        err = (suites[i])();
        if (err != CUE_SUCCESS) goto CLEANUP;
    }

    // Start the tests
    CU_basic_set_mode(runMode);
    PI_CheckLevel = 3;		// use max. error checking (default=1)
    CU_basic_run_tests();

CLEANUP:
    CU_cleanup_registry();
    MPI_Finalize();
    return err;
}

PI_PROCESS* CreateAliasedProcess(PI_WORK_FUNC f, const char *alias, int index,
                                 void *opt_pointer)
{
    PI_PROCESS* proc = PI_CreateProcess(f, index, opt_pointer);
    PI_SetName(proc, alias);
    return proc;
}

static void dummyTest(void)
{}

void AddTest(CU_pSuite suite, const char* name, CU_TestFunc func)
{
    // We only want to run the tests on Master. All the other procs will be
    // participating in the tests, but should not be explicitly starting them.
    //
    // If this isn't rank 0, a dummy test function is added to make sure that
    // the suite's init and cleanup functions still get called.
    if (my_rank == 0)
        CU_add_test(suite, name, func);
    else
        CU_add_test(suite, name, dummyTest);
}
