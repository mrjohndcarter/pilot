/*
Unit tests for the format parser. This suite indirectly tests ParseFormatString
through PI_Read and PI_Write.

NOTE: you should *not* run this suite with the deadlock detector on. It depends
on the fact that we can PI_Write without a receiver executing PI_Read, as long
as deadlock checking isn't putting PI_Write into synchronous mode.

For V2.0, the PI_FORMAT_INVALID and PI_ARRAY_LENGTH errors are now being issued
in some of these cases rather than the former catch-all PI_FORMAT_ARGS.

For V2.1, now checking that bogus pointers can be detected.
*/
#include "unittests.h"

#include "mpi.h"	// for MPI_SUM & MPI_FLOAT

PI_CHANNEL *dummy_chan, *dummy_rchan;
PI_PROCESS *dummy_proc;

PI_CHANNEL *bund_chan[1];
PI_BUNDLE *reducer_bund;
PI_PROCESS *reducer_proc;

PI_CHANNEL *all_types_chan;
PI_PROCESS *all_types_proc;


// This worker process is at the end of dummy_[r]chan, but it does not ever
// send/recv any messages because the PI_Write/Read instances are all erroneous,
// therefore it is OK for this function to do nothing and just exit.
static int dummy_process_func( int arg1, void* arg2 )
{
    return 0;
}

static int reducer_func( int arg1, void* arg2 )
{
    int intv;
    float floatv;

    PI_Reduce(
	reducer_bund,
	"%max/d %min/d %+/f %*/f %&&/d %||/d %^^/d %&/d %|/d %^/d %mop/d",
	&intv, &intv, &floatv, &floatv, &intv, &intv, &intv, &intv, &intv, &intv, MPI_SUM, &intv
    );
    return 0;
}

static int all_types_func( int arg1, void* arg2 )
{
    unsigned char b, hhu;
    char c;
    short hd;
    int d;
    long int ld;
    long long int lld;
    unsigned short hu;
    unsigned long lu;
    unsigned long long llu;
    float f;
    double lf;
    long double Lf;
    float mpi_float;

    // Since we're not checking the contents of each variable, some of the above
    // variables appear in the argument list multiple times (where synonyms are
    // being read).
    PI_Read(
        all_types_chan,
        "%b %c %hi %hd %d %i %ld %li %lld %lli %hhu %hu",
        &b, &c, &hd, &hd, &d, &d, &ld, &ld, &lld, &lld, &hhu, &hu
    );
    
    PI_Read(
        all_types_chan,
        "%lu %llu %f %lf %Lf %m",
        &lu, &llu, &f, &lf, &Lf, MPI_FLOAT, &mpi_float
    );
    return 0;
}

static void ShouldFailOnWhitespaceOnly( void )
{
    PI_Errno = 0;
    PI_Write( dummy_chan, "  \t   ", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );

    PI_Errno = 0;
    PI_Write( dummy_chan, "", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );

    PI_Errno = 0;
    PI_Write( dummy_chan, "\n\r \v\t", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );
}

static void ShouldFailOnInvalidSizeArray( void )
{
    int a[1] = {0};
    const char* fmts[] = { "%1d", "%1ld", "%0d", NULL };
    int i;

	// These hardcoded lengths are not valid: 0 and 1
    for ( i = 0; fmts[i] != NULL; i++ ) {
        PI_Errno = 0;
        PI_Write( dummy_chan, fmts[i], a );
        CU_ASSERT_EQUAL( PI_Errno, PI_ARRAY_LENGTH );
    }

    // Zero and negative lengths not allowed
    for ( i = 0; i >= -3; i-- ) {
        PI_Errno = 0;
        PI_Write( dummy_chan, "%*d", i, a );
        CU_ASSERT_EQUAL( PI_Errno, PI_ARRAY_LENGTH );
    }
    
    // Try that again with V2.1's variable length array
    for ( i = 0; i >= -3; i-- ) {
        PI_Errno = 0;
        PI_Write( dummy_chan, "%^d", i, a );
        CU_ASSERT_EQUAL( PI_Errno, PI_ARRAY_LENGTH );
    }

    // Only one length can be specified
    PI_Errno = 0;
    PI_Write( dummy_chan, "%3*d", 1, a );
    CU_ASSERT_EQUAL( PI_Errno, PI_ARRAY_LENGTH );

	// Negative sign should be invalid format
    PI_Errno = 0;
    PI_Write( dummy_chan, "%-1d", 1, a );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );
}

static void ShouldFailOnLolByob( void )
{
    // An old version of Pilot would parse %lol%%byob as if it were %lf%b
    PI_Errno = 0;
    PI_Write( dummy_chan, "%lol%%byob", 2.0f, 'b' );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );
}

static void ShouldNotAcceptPartialConversionSpec( void )
{
    PI_Errno = 0;
    PI_Write( dummy_chan, " %", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );
}

static void ShouldFailOnNullFormatString( void )
{
    PI_Errno = 0;
    PI_Write( dummy_chan, NULL, 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_NULL_FORMAT );
}

static void ShouldAcceptBasicFormats( void )
{
    float f = 11.0f;
    PI_Errno = 0;
    
    // WARNING!!! At least one compiler had trouble pulling the "long double" value
    // via va_args when it appeared far down the arg list, so the former single test
    // has been broken into two parts.
    
    PI_Write(
        all_types_chan,
        "%b %c %hi %hd %d %i %ld %li %lld %lli %hhu %hu",
        'b', 'c', (short) 1, (short) 1, 2, 2, 3L, 3L, 4LL, 4LL, 'u', (unsigned short) 5
    );
    CU_ASSERT_EQUAL( PI_Errno, 0 );

    PI_Write(
        all_types_chan,
        "%lu %llu %f %lf %Lf %m",
        6UL, 7ULL, 8.0f, 9.0, (long double) 10,
        MPI_FLOAT, &f
    );
    CU_ASSERT_EQUAL( PI_Errno, 0 );
}

// Next test assumes that 55 formats/args > PI_MAX_FORMATLEN, so the error PI_FORMAT_ARGS
// will result.  Check that with the following directives:
#if (PI_MAX_FORMATLEN >= 55)
#error ShouldNotCrashWithLongFormat test case does not have enough args to trigger error
#endif

static void ShouldNotCrashWithLongFormat( void )
{
    PI_Errno = 0;
    PI_Write( dummy_chan, "%d%d%d%d%d %d%d%d%d%d %d%d%d%d%d %d%d%d%d%d "
        "%d%d%d%d%d %d%d%d%d%d %d%d%d%d%d %d%d%d%d%d %d%d%d%d%d %d%d%d%d%d %d%d%d%d%d",
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55
    );
    // If we get here, then the test is mostly a success regardless of the assert
    // Assert assumes that 55 args > PI_MAX_FORMATLEN
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_ARGS );
}
  

static void ShouldAcceptReduceOps( void )
{
    PI_Errno = 0;
    PI_Write(
	bund_chan[0],
	"%max/d %min/d %+/f %*/f %&&/d %||/d %^^/d %&/d %|/d %^/d %mop/d",
	1, 2, (float)3.1, (float)3.2, 1, 0, 1, 888, 777, 666, MPI_SUM, 999
    );
    CU_ASSERT_EQUAL( PI_Errno, 0 );
}

static void ReduceOpsWithBundle( void )
{
    // Reducer bundle should insist on reduce op
    PI_Errno = 0;
    PI_Write( bund_chan[0], "%d", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_OP_MISSING );

    // Ordinary channel should not accept reduce op
    PI_Errno = 0;
    PI_Write( dummy_chan, "%+/d", 1 );
    CU_ASSERT_EQUAL( PI_Errno, PI_OP_INVALID );
}

static void ShouldDetectWrongNumArgs( void )
{
    int seven[6] = {0};
    
    // Too few args for formats
    PI_Errno = 0;
    PI_Write( dummy_chan, "%d%d%d%d%d %^d %s", 1, 2, 3, 4, 5, 6, seven);
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_ARGS );
    
    // Too many args for formats
    PI_Errno = 0;
    PI_Write( dummy_chan, "%d%d%d%d%d %^d %s", 1, 2, 3, 4, 5, 6, seven, "8", 9);
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_ARGS );
}

static void ShouldDetectBogusPointers( void )
{
    int stack = 100;
    float stackf[5] = {0};
    static char bss[10] = {'A'};
    static char *data = "data seg";
    double *heap;
    
    /* For tests on dummy channel, in the event that the error is not
     * detected, we don't want the read/write to proceed because that would hang (due to
     * dummy process is not writing/reading).  So we throw in a later error, an invalid
     * format code, that will be detected if the bogus pointer sneaks through.
     */
    
    // Typical fscanf-type error of forgetting the "&" on scalar read
    PI_Errno = 0;
    PI_Read( dummy_rchan, "%d %BAD", stack );
    CU_ASSERT_EQUAL( PI_Errno, PI_BOGUS_POINTER_ARG );
    
    // Read into array needs address of first location
    PI_Errno = 0;
    PI_Read( dummy_rchan, "%10c %BAD", bss[0] );
    CU_ASSERT_EQUAL( PI_Errno, PI_BOGUS_POINTER_ARG );
    
    // Write from array
    PI_Errno = 0;
    PI_Write( dummy_chan, "%10c %BAD", bss[0] );
    CU_ASSERT_EQUAL( PI_Errno, PI_BOGUS_POINTER_ARG );
    
    // Make sure a variety of good addresses are accepted (should reject %BAD)
    heap = malloc(10 * sizeof(double));
    PI_Errno = 0;
    PI_Write( dummy_chan, "%*f %10c %s %10d %BAD", 5, stackf, bss, data, heap );
    CU_ASSERT_EQUAL( PI_Errno, PI_FORMAT_INVALID );
    free(heap);
}

static int init(void)
{
    int argc = default_argc;
    char** argv = default_argv;
    PI_QuietMode = 1;
    PI_OnErrorReturn = 1;

    PI_Configure(&argc, &argv);

    dummy_proc = PI_CreateProcess( dummy_process_func, 0, NULL );
    dummy_chan = PI_CreateChannel( PI_MAIN, dummy_proc );
    dummy_rchan = PI_CreateChannel( dummy_proc, PI_MAIN );

    reducer_proc = PI_CreateProcess( reducer_func, 0, NULL );
    bund_chan[0] = PI_CreateChannel( PI_MAIN, reducer_proc );
    reducer_bund = PI_CreateBundle( PI_REDUCE, bund_chan, 1 );

    all_types_proc = PI_CreateProcess( all_types_func, 0, NULL );
    all_types_chan = PI_CreateChannel( PI_MAIN, all_types_proc );

    PI_StartAll();
    return 0;
}

static int cleanup(void)
{
    if (my_rank == 0)
        PI_StopMain(0);
    return 0;
}

CU_ErrorCode AddFormatSuite(void)
{
    CU_pSuite suite = CU_add_suite( "Format Parsing", init, cleanup );
    if (suite == NULL)
        return CU_get_error();

    AddTest( suite, "Should fail on whitespace only format", ShouldFailOnWhitespaceOnly );
    AddTest( suite, "Should fail with invalid sized array", ShouldFailOnInvalidSizeArray );
    AddTest( suite, "Should fail to parse \"%lol%%byob\"", ShouldFailOnLolByob );
    AddTest( suite, "Should not accept an incomplete conversion spec", ShouldNotAcceptPartialConversionSpec );
    AddTest( suite, "Should fail on NULL format string", ShouldFailOnNullFormatString );
    AddTest( suite, "Try all format codes", ShouldAcceptBasicFormats );
    AddTest( suite, "Should not crash with too long format string", ShouldNotCrashWithLongFormat );
    AddTest( suite, "Try all reduce operators", ShouldAcceptReduceOps );
    AddTest( suite, "Should only accept reduce operators with reducer bundle", ReduceOpsWithBundle );
    AddTest( suite, "Should detect too few/many arguments for formats", ShouldDetectWrongNumArgs );
    AddTest( suite, "Should detect a variety of bogus pointers for reading/writing", ShouldDetectBogusPointers );

    return CUE_SUCCESS;
}
