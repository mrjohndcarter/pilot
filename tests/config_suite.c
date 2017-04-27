/*
Configuration tests for Pilot. These test error detection for misuse of
configuration phase API calls.
*/
#include "unittests.h"


static int dummy(int index, void* arg2)
{
    /* this function never actually runs */
    return 0;
}

static void pi_create_bundle_errors(void)
{
    PI_PROCESS *w[2];
    PI_CHANNEL *chans[2];

    w[0] = CreateAliasedProcess(dummy, "dummy", 0, NULL);
    w[1] = CreateAliasedProcess(dummy, "dummy", 1, NULL);
    chans[0] = PI_CreateChannel(w[0], PI_MAIN);
    chans[1] = PI_CreateChannel(w[1], PI_MAIN);

    /* BUG #28: CreateBundle fails to detect reincorporation of same
	channels into 2nd bundle.
	- make bundle with chans 0 & 1
	- make another bundle with same chans => PI_BUNDLE_ALREADY error
    */
    PI_CreateBundle(PI_SELECT, chans, 2);

    PI_Errno = 0;
    PI_CreateBundle(PI_GATHER, chans, 2);

    CU_ASSERT_EQUAL( PI_Errno, PI_BUNDLE_ALREADY );
}

static int init(void)
{
    int argc = default_argc;
    char** argv = default_argv;
    PI_QuietMode = 1;
    PI_OnErrorReturn = 1;

    PI_Configure(&argc, &argv);

    // Configuration code here is in test functions
    // StartAll is in cleanup

    return 0;
}

static int cleanup(void)
{
    if ( 0==PI_StartAll() )	// only call for PI_MAIN (rank 0)
        PI_StopMain(0);
    return 0;
}

CU_ErrorCode AddConfigSuite(void)
{
    CU_pSuite suite = CU_add_suite("Configuration Tests", init, cleanup);
    if (suite == NULL)
        return CU_get_error();

    AddTest(
        suite,
        "PI_CreateBundle should fail in various circumstances",
        pi_create_bundle_errors
    );

    return CUE_SUCCESS;
}
