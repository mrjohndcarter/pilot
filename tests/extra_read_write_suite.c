/*
Tests for PI_Read and PI_Write where these functions are expected to abort with
a specified error code from pilot_error.h.
*/
#include "unittests.h"

static PI_PROCESS *test8_r, *test8_w, *test8_w2;
static PI_CHANNEL *test8_r_chans[1], *test8_w_chans[2];
static PI_BUNDLE *test8_r_bundle, *test8_w_bundle;

static int test8_reader(int index, void* arg2)
{
    float recv;
    PI_Read(test8_r_chans[index], "%f", &recv);
    return 0;
}

static int test8_writer(int index, void* arg2)
{
    long send = 21;
    PI_Write(test8_w_chans[index], "%ld", send);
    return 0;
}

static int test8_writer2(int index, void* arg2)
{
    char send = 'A';
    PI_Write(test8_w_chans[index], "%c %c", send, send);
    return 0;
}

static void pi_write_fail_on_bundled_channel(void)
{
    PI_Errno = 0;
    PI_Write(test8_r_chans[0], "%f", (float)1.2);

    CU_ASSERT_EQUAL(PI_Errno, PI_BUNDLED_CHANNEL);

    // unblock the reading process
    if (PI_Errno != 0)
        PI_Broadcast(test8_r_bundle, "%f", (float)1.2);
}

static void pi_read_fail_on_bundled_channel(void)
{
    long recv;
    PI_Errno = 0;
    PI_Read(test8_w_chans[0], "%ld", &recv);

    CU_ASSERT_EQUAL(PI_Errno, PI_BUNDLED_CHANNEL);

    // unblock the writing process
    if (PI_Errno != 0)
        PI_Gather(test8_w_bundle, "%ld", &recv);
}

static void pi_read_fail_on_too_few_args(void)
{
    char recv;
    PI_Errno = 0;
    PI_Read(test8_w_chans[1], "%c %c", &recv);

    CU_ASSERT_EQUAL(PI_Errno, PI_FORMAT_ARGS);
}

static int init(void)
{
    int argc = default_argc;
    char** argv = default_argv;
    PI_QuietMode = 1;
    PI_OnErrorReturn = 1;

    PI_Configure(&argc, &argv);

    test8_r = CreateAliasedProcess(test8_reader, "test8_reader", 0, NULL);
    test8_w = CreateAliasedProcess(test8_writer, "test8_writer", 0, NULL);
    test8_w2 = CreateAliasedProcess(test8_writer2, "test8_writer2", 1, NULL);

    test8_r_chans[0] = PI_CreateChannel(PI_MAIN, test8_r);
    test8_r_bundle = PI_CreateBundle(PI_BROADCAST, test8_r_chans, 1);

    test8_w_chans[0] = PI_CreateChannel(test8_w, PI_MAIN);
    test8_w_bundle = PI_CreateBundle(PI_GATHER, test8_w_chans, 1);

    test8_w_chans[1] = PI_CreateChannel(test8_w2, PI_MAIN);

    PI_StartAll();
    return 0;
}

static int cleanup(void)
{
    if (my_rank == 0)
        PI_StopMain(0);
    return 0;
}

CU_ErrorCode AddExtraReadWriteSuite(void)
{
    CU_pSuite suite = CU_add_suite("Extra read/write tests", init, cleanup);
    if (suite == NULL)
        return CU_get_error();

    AddTest(
        suite,
        "PI_Write should fail when channel is part of non-selector bundle",
        pi_write_fail_on_bundled_channel
    );

    AddTest(
        suite,
        "PI_Read should fail when channel is part of non-selector bundle",
        pi_read_fail_on_bundled_channel
    );

    AddTest(
        suite,
        "PI_Read should fail when too few args supplied",
        pi_read_fail_on_too_few_args
    );

    return CUE_SUCCESS;
}
