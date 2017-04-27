/*
Tests for PI_Scatter (V2.0).

Tests that:
 - scattering is possible to 3 processes.
 - scattering a large array (> 10000 ints) does not cause problems.
 - it is possible to scatter from a process other than PI_MAIN.
*/
#include "unittests.h"
#include <stdio.h>

PI_PROCESS *test11_1, *test11_2, *test11_3;
PI_CHANNEL *from_test11[3], *from_test11c[2];
PI_CHANNEL **to_test11;
PI_BUNDLE *test11_bundle;

PI_PROCESS *test11_4;
PI_CHANNEL *test11_large_array_channels[1], *test11_large_array_sum;
PI_BUNDLE *test11_large_array_bundle;

static int scatter_read(int q, void *p)
{
    char c[1];

    // read scattered data, then echo back
    PI_Read(from_test11[q],"%*c", 1, c);
    PI_Write(to_test11[q], "%*c", 1, c);

    return 0;
}

static void test11a(void)
{
    int i;
    char cc[3] = {'A', 'B', 'C'}, cback;

    PI_Scatter(test11_bundle,"%*c", 1, cc);

    for (i=0; i<3; i++) {
	PI_Read(to_test11[i], "%*c", 1, &cback);
	CU_ASSERT_EQUAL(cback,'A'+i);
    }
}

static int scatter_array_write(int idx, void* arg2)
{
    int i, sum=0;
    int len = 10001;
    int* ints = malloc(sizeof(int) * len);
    if (ints == NULL) {
        perror("scatter_array_write()");
        return -1;
    }

    PI_Read(test11_large_array_channels[idx], "%*d", len, ints);
    for (i=0; i<len; i++) {
	sum += ints[i];
    }
    PI_Write(test11_large_array_sum, "%d", sum);

    free(ints);
    return 0;
}

static void test11b(void)
{
    int i, sum=0, checksum=0;
    int len = 10001;
    int* ints = malloc(sizeof(int) * len);
    if (ints == NULL) {
        CU_FAIL_FATAL("Unable to allocate memory for test11b");
    }
    for (i=0; i<len; i++) {
	ints[i] = i;
	sum += i;
    }

    // scatter the array (all to one process) then read back sum
    PI_Scatter(test11_large_array_bundle, "%*d", len, ints);
    PI_Read(test11_large_array_sum, "%d", &checksum);
    CU_ASSERT_EQUAL(sum, checksum);
    free(ints);
}

PI_PROCESS *test11c_w[2], *test11c_nonmain;
PI_CHANNEL *from_nonmain_scatterer[2], *to_test11c[2];
PI_BUNDLE  *nonmain_bundle;

static int test11c_writer(int proc_id, void* arg2)
{
    int i, ints[5] = {0}, sum=0;

    PI_Read(from_nonmain_scatterer[proc_id], "%5d", ints);
    for (i = 0; i < 5; i++)
	sum += ints[i];
    PI_Write(to_test11c[proc_id], "%d", sum);
    return 0;
}
    int i;

static int nonmain_scatterer(int arg1, void* arg2)
{
    int ints[2][5] = { {1,2,3,4,5}, {6,7,8,9,10} };

    PI_Scatter(nonmain_bundle, "%*d", 5, ints);
    return 0;
}

/* non-PI_MAIN scatterer
- nonmain_scatterer sends data to test11c_writer[0..1]
- they add their buffers and send sums to test11c
*/
static void test11c(void)
{
    int sum0, sum1;
    PI_Read(to_test11c[0], "%d", &sum0);
    PI_Read(to_test11c[1], "%d", &sum1);
    CU_ASSERT( sum0+sum1 == 55 );
}

static int init(void)
{
    int argc = default_argc;
    char** argv = default_argv;
    PI_QuietMode = 1;
    PI_OnErrorReturn = 1;

    PI_Configure(&argc, &argv);

    test11_1 = CreateAliasedProcess(scatter_read,"test11",0,NULL);
    test11_2 = CreateAliasedProcess(scatter_read,"test11",1,NULL);
    test11_3 = CreateAliasedProcess(scatter_read,"test11",2,NULL);

    from_test11[0] = PI_CreateChannel(PI_MAIN,test11_1);
    from_test11[1] = PI_CreateChannel(PI_MAIN,test11_2);
    from_test11[2] = PI_CreateChannel(PI_MAIN,test11_3);
    to_test11 = PI_CopyChannels(PI_REVERSE,from_test11,3);

    test11_bundle = PI_CreateBundle(PI_SCATTER, from_test11,3);
    PI_SetName(test11_bundle, "test11 scatterer");

    test11_4 = CreateAliasedProcess(scatter_array_write, "test11_4", 0, NULL);
    test11_large_array_channels[0] = PI_CreateChannel(PI_MAIN,test11_4);
    test11_large_array_bundle = PI_CreateBundle(PI_SCATTER, test11_large_array_channels, 1);
    test11_large_array_sum = PI_CreateChannel(test11_4,PI_MAIN);

    test11c_w[0] = CreateAliasedProcess(test11c_writer, "test11c_writer0", 0, NULL);
    test11c_w[1] = CreateAliasedProcess(test11c_writer, "test11c_writer1", 1, NULL);
    test11c_nonmain = CreateAliasedProcess(nonmain_scatterer, "nonmain_scatterer", 0, NULL);
    from_nonmain_scatterer[0] = PI_CreateChannel(test11c_nonmain, test11c_w[0]);
    from_nonmain_scatterer[1] = PI_CreateChannel(test11c_nonmain, test11c_w[1]);
    nonmain_bundle = PI_CreateBundle(PI_SCATTER, from_nonmain_scatterer, 2);
    to_test11c[0] = PI_CreateChannel(test11c_w[0], PI_MAIN);
    to_test11c[1] = PI_CreateChannel(test11c_w[1], PI_MAIN);

    PI_StartAll();
    return 0;
}

static int cleanup(void)
{
    free(to_test11);

    if (my_rank == 0)
        PI_StopMain(0);
    return 0;
}

CU_ErrorCode AddScattererSuite(void)
{
    CU_pSuite suite = CU_add_suite("Scatterer Tests", init, cleanup);
    if (suite == NULL)
        return CU_get_error();

    AddTest(suite, "scatterer tests", test11a);
    AddTest(suite, "scatterer large array", test11b);
    AddTest(suite, "non-main scatterer", test11c);

    return CUE_SUCCESS;
}
