/*
Tests for PI_Reduce (V2.0).

Tests that:
 - reducing scalars is possible from 4 processes.
 - reducing large arrays (10000 ints) does not cause problems.
 - it is possible to reduce to a process other than PI_MAIN.
*/
#include "unittests.h"
#include <mpi.h>

PI_PROCESS *test12_1, *test12_2, *test12_3, *test12_4, *test12_nonmain;
PI_CHANNEL *from_test12[4], *from_test12c[4], *from_nonmain;
PI_BUNDLE *test12_bundle, *test12c_bundle;

MPI_Op test12_mop;	// user-defined reduce operation for passing to PI_Reduce

/* This is silly a user-defined reduce operation. It calculates the "evenness" or "oddness" of
 * a pair of numbers:  2 evens or 2 odds => even (0); 1 odd or 1 even => odd (1). It is
 * commutative, and only written to work on integers.
 */
static void oddness(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype) {
  
    int i;

    // do pairwise combo, writing output into inoutvec
    for ( i=0; i<*len; i++ ) {
	((int *)inoutvec)[i] = ( ((int *)invec)[i] % 2 + ((int *)inoutvec)[i] % 2 ) % 2;
    }
  
    return;  
}

static int reduce_write(int q, void *p) {

    // Test 12a: basic reduction
    float sum[4] = {130.85, -15.01, 3590.103, -2589.3};
    int product[4] = {67, 23, 158, -10};
    long maxx[4] = { 256, -92873, 99999, 4};

    // try 3 different operations: sum, product, max
    PI_Write(from_test12[q], "%+/f %*/d %max/ld", sum[q], product[q], maxx[q]);

#define LARGE_LEN 10000
    int large[LARGE_LEN], i;
    
    for ( i=0; i<LARGE_LEN; i++ )
	large[i] = q*LARGE_LEN + i;
    
    // Test 12b: reduce large array
    PI_Write(from_test12[q], "%+/*d", LARGE_LEN, large);
    
    // Test 12c: reduce to non-main process, with user-defined operator
    int numbers[4] = {625, 1033, 4444, 9};	// result should be ODD
    PI_Write(from_test12c[q], "%mop/d", test12_mop, numbers[q]);

    return 0;
}

static void test12a(void) {
    
    float good_sum = (float)( 130.85 - 15.01 + 3590.103 - 2589.3);
    int good_product = 67 * 23 * 158 * -10;
    long good_maxx = 99999;

    float sum;
    int product;
    long maxx;

    PI_Reduce(test12_bundle, "%+/f %*/d %max/ld", &sum, &product, &maxx);

    CU_ASSERT_DOUBLE_EQUAL(good_sum,sum,0.00001);
    CU_ASSERT_EQUAL(good_product,product);
    CU_ASSERT_EQUAL(good_maxx,maxx);
}

static void test12b(void) {
    
    int large[LARGE_LEN] = {0};	// initialize to 0's
    int good_elmt, i;

    PI_Reduce(test12_bundle, "%+/*d", LARGE_LEN, large);

    for ( i=0; i<LARGE_LEN; i++ ) {
        good_elmt = (1+2+3)*LARGE_LEN + 4*i;
	CU_ASSERT_EQUAL(good_elmt,large[i]);
    }
}

static int nonmain(int arg1, void* arg2)
{
    int result = -1;

    // pass the operation along with the result location
    PI_Reduce(test12c_bundle, "%mop/d", test12_mop, &result);
  
    // send result on to PI_MAIN
    PI_Write(from_nonmain, "%d", result);
    return 0;
}

static void test12c(void)
{
    int odd, good_odd=(625+1033+4444+9) % 2;

    PI_Read(from_nonmain, "%d", &odd);
    CU_ASSERT_EQUAL(good_odd,odd);
}

static int init(void)
{
    int argc = default_argc;
    char** argv = default_argv;
    PI_QuietMode = 1;
    PI_OnErrorReturn = 1;

    PI_Configure(&argc, &argv);

    test12_1 = CreateAliasedProcess(reduce_write,"test12",0,NULL);
    test12_2 = CreateAliasedProcess(reduce_write,"test12",1,NULL);
    test12_3 = CreateAliasedProcess(reduce_write,"test12",2,NULL);
    test12_4 = CreateAliasedProcess(reduce_write,"test12",3,NULL);
    test12_nonmain = CreateAliasedProcess(nonmain,"test12c",0,NULL);

    from_test12[0] = PI_CreateChannel(test12_1,PI_MAIN);
    from_test12[1] = PI_CreateChannel(test12_2,PI_MAIN);
    from_test12[2] = PI_CreateChannel(test12_3,PI_MAIN);
    from_test12[3] = PI_CreateChannel(test12_4,PI_MAIN);

    from_test12c[0] = PI_CreateChannel(test12_1,test12_nonmain);
    from_test12c[1] = PI_CreateChannel(test12_2,test12_nonmain);
    from_test12c[2] = PI_CreateChannel(test12_3,test12_nonmain);
    from_test12c[3] = PI_CreateChannel(test12_4,test12_nonmain);
    
    from_nonmain = PI_CreateChannel(test12_nonmain,PI_MAIN);

    // V3.1 problem: Jumpshot crashes with long bundle names, shorten for now
    test12_bundle = PI_CreateBundle(PI_REDUCE, from_test12, 4);
    PI_SetName(test12_bundle, "T12" /*"test12 reducer"*/);
    
    test12c_bundle = PI_CreateBundle(PI_REDUCE, from_test12c, 4);
    PI_SetName(test12c_bundle, "T12C" /*"test12c reducer"*/);

    // create the reduction operation from the oddness() function, needed for test c
    MPI_Op_create( oddness, 1, &test12_mop );

    PI_StartAll();
    return 0;
}

static int cleanup(void)
{
    MPI_Op_free( &test12_mop );
    
    if (my_rank == 0)
        PI_StopMain(0);
    return 0;
}

CU_ErrorCode AddReducerSuite(void)
{
    CU_pSuite suite = CU_add_suite("Reducer Tests", init, cleanup);
    if (suite == NULL)
        return CU_get_error();

    AddTest(suite, "reducer tests", test12a);
    AddTest(suite, "reducer large array", test12b);
    AddTest(suite, "non-main reducer", test12c);
    
    return CUE_SUCCESS;
}
