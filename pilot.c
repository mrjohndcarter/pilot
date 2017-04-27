/***************************************************************************
 * Copyright (c) 2008-2017 University of Guelph.
 *                         All rights reserved.
 *
 * This file is part of the Pilot software package.  For license
 * information, see the LICENSE file in the top level directory of the
 * Pilot source distribution.
 **************************************************************************/
// This file contains some tags for use by the astyle formatting program

/*!
********************************************************************************
\file pilot.c
\author John Douglas Carter
\author Bill Gardner
\author Emmanuel MacCaull
\author Daniel Recoskie
\author Kevin Green
\author Jordan Goldberg
\author Sid Bao

\brief Implementation file for Pilot

[9-Mar-09] Fixed bug #21: PI_Gather triggers stack out-of-bounds seg fault
	with large array. Switched to MPI_Gatherv. V0.1 (BG)
[30-May-09] Fixed bug #25: PI_Read/Write doesn't realize when channel part
	of non-Selector bundle. V1.0 (BG)
[10-July-09] Fixed bug #22,#23: Too-long format string can overflow buffer;
	ParseFormatStrings not picky enough. V1.0 (EM)
[22-Nov-09] Add support for Fortran data types. V1.1f (BG)
[23-Nov-09] Allow PI_GetName to be called anytime. V1.1f (BG)
[6-Dec-09] Use MPI_Ssend when deadlock checking enabled to match CSP unbuffered
	channel semantics and prevent "false positives". V1.1f (BG)
[9-Jan-10] Fixed #33: Fortran data type symbols missing from mpi.h in
	Open MPI. Check MPID_NO_FORTRAN. Also, no MPI_IN_PLACE in LAM. V1.2 (BG)
[16-Jan-10] *Some* Fortran data type symbols missing from mpi.h in HP MPI.
	#define *most* missing ones following the pattern for MPI_INTEGER,
	and the "new ones" from mpif.h. V1.2a (BG)
[13-May-11] Fixed #28: CreateBundle fails to detect reincorporation of same
	channels into 2nd bundle. Needed new error PI_BUNDLE_ALREADY. V2.0 (BG)
[15-May-11] Fixed #38: PI_GetMyRank was undocumented API, only used by test
	suites, which didn't really need it => removed. V2.0 (BG)
[15-May-11] Fixed #30: confusing for a corrupted object in ISVALID assertion
	to throw "system error". Change cases where arg points to invalid object
	to PI_INVALID_OBJ (could be wrong pointer or corruption), and make
	new PI_MPI_ERROR for that case (could be misuse of MPI by Pilot, or
	MPI detecting user error). V2.0 (BG)
[16-May-11] Fixed #31: Try to keep Pilot messages separate from MPI msgs to
	avoid confusion. Print using new PI_BORDER macro (pilot_private.h).
	V2.0 (BG)
[25-May-11] Added PI_Scatter & PI_Reduce.
	Add to Fortran API. V2.0 (BG)
[22-Aug-11] Added PI_SetCommWorld to accommodate IMB benchmarks. V2.1 (DR)
[2-Sep-13] Added variable length data formats (^ flag and %s type). V2.1 (KG/JG)
[4-Sep-13] Clear LogEvent's buffer so valgrind doesn't find uninitialized storage.
	V2.1 (BG)
[26-Sep-13] Make OnlineProcessFunc flush after every write to avoid losing log contents
	in event of PI_Abort or crash. V2.1 (BG)
[27-Sep-13] Improve call logging by adding arg interpretation and avoiding buffer
	overflow (also see pilot_private.h LOGCALL). V2.1 (BG)
[1-Oct-13] Replace end-of-arg list sentinels with variadic macro arg counting
	to detect mismatch between formats and arg list. V2.1 (BG)
[1-May-14] Fix deprecations from MPI-2.0 & 2.2. V3.0 (BG)
[5-May-14] Add level 3 checking for read/write pointer args, including regression
	tests. V3.0 (BG)
[6-May-14] Add level 2 matching of read/write formats, including regression
	tests. Move logging (for deadlock detection) to before the format
	check, so the check messages don't cause an undetected deadlock. V3.0 (BG)
[14-Feb-16] Add MPE calls to implement logging for Jumpshot, all conditionally
        compiled on PILOT_WITH_MPE. V3.1 (SB)
[9-May-16] Add -pisvc "j" flag for Jumpshot-compatible log (via MPE); change -pilog
        to use .ext if provided, otherwise as base name (appropriate .ext added). V3.1 (BG)
[10-May-16] Pulled out the code that partially implemented the online process
        running as a pthread on rank 0. This was never completed due to lack of reliable
        threading support in some MPIs, and now it's trying to join with a non-existent
        thread when MPE is in use. So -pilog=t or p is no longer recognized. V3.1 (BG)
[29-Jul-16] Enforce PI_Start/EndTime use only after PREINIT phase to insure that
        MPE is running if enabled, and to allow MPE to access caller's line no. V3.1 (BG)
[2-Feb-17] Fix problem of long names failing MPE_Log_pack, causing Jumpshot to crash
        when object is rt-clicked. V3.1 (BG)
*******************************************************************************/

#include "pilot_private.h"	// include these typedefs first

#define PI_NO_OPAQUE		// suppress typedefs in public pilot.h
#include "pilot.h"

#define DEF_ERROR_TEXT      // generate storage for error text array
#include "pilot_error.h"

/* headers for online processes */
#include "pilot_deadlock.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>  //for usleep()

#ifdef PILOT_WITH_MPE
#include <mpe.h>
#endif

/* MPI version oddities
 *
 * MPI_Comm_errhandler_fn was deprecated in MPI 2.2, but an earlier
 * implementation may not have defined its replacement, so change back to the
 * legacy call if necessary.
 */
#if MPI_VERSION<2 || (MPI_VERSION==2 && MPI_SUBVERSION<2)
#define MPI_Comm_errhandler_function MPI_Comm_errhandler_fn
#endif
/*
 * MPI_Send and _Ssend changed their first arg from "void *" to "const void *".
 * Make a typedef to the new signature so we can cast the old one.
 */
typedef int (MPI_Send_func)(const void*, int, MPI_Datatype, int, int, MPI_Comm);

/*** Pilot global variables ***/

int PI_QuietMode = 0;		// quiet mode off
int PI_CheckLevel = 1;		// default level of checking
int PI_OnErrorReturn = 0;	// on any error, abort program
int PI_Errno = PI_NO_ERROR;	// error code returned here (if no abort)
const char *PI_CallerFile;	// filename of caller (set by macro)
int PI_CallerLine;		// line no. of caller (set by macro)
MPI_Comm PI_CommWorld = MPI_COMM_WORLD;	// MPI communicator comprising all available ranks

/* MPE global variables
 *
 * bytebuf is packed with binary data by MPE_Log_pack and its size is strictly
 * limited (32 bytes).  If it were only used for a single string (i.e., no other
 * numeric/string items), the max string length would be 30, because 2 bytes are
 * taken by the binary string length.
 *
 * mybuff is used with interpArg, and won't produce more than 30 bytes of text,
 * so it can use the same typedef as bytebuf.  The exact limit in each context
 * is given by a LOG_xxx_SMAX symbol which is guaranteed to be <= 30.
 */
#ifdef PILOT_WITH_MPE
static MPE_LOG_BYTES bytebuf, mybuff;   // these are char[MPE_LOG_BYTESIZE]
static int bytebuf_pos;         // next free char position in bytebuf

/*** MPE extra functions ***/
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/*** Forward declarations of internal-use functions ***/
static void HandleMPIErrors( MPI_Comm *comm, int *code, ... );
static char *ParseArgs( int *argc, char ***argv );
static int OnlineProcessFunc( int a1, void *a2 );
static const char *interpArg( char *dest, int maxlen, const char *code, const PI_MPI_RTTI *arg );
static uint32_t FormatSignature( PI_MPI_RTTI meta[], int items );
static int ParseFormatString( IO_CONTEXT valsOrLocs, PI_MPI_RTTI meta[], const char *fmt, va_list ap );

/*** Pointer validation function ***/
static int CheckPointer( void *ptr );
static void *ArgvCopy;		// needed by CheckPointer, set by PI_Configure
// NOTE: See pilot_private.h for externs and feature test macro

/*** Logging facility ***/
typedef enum { PILOT='P', USER='U', TABLES='T', CALLS='C', STATS='S' } LOGEVENT;
static void LogEvent( LOGEVENT ev, const char *event );


#define LOUD if( !PI_QuietMode )


/*!
********************************************************************************
\brief Copy of the environment for this Pilot process.

Holds processes, channels and internal variables.  Each process
will possess the same environment at PI_StartAll(), then local updates will
occur for bookkeeping purposes as reads/writes take place.
*******************************************************************************/
static PI_PROCENVT thisproc = { .phase=PREINIT };

static int MPICallLine;	/*!< line number of last MPI library call (PI_CALLMPI macro) */
static int MPIMaxTag;	/*!< max tag number allowed by this MPI implementation */
static int MPIPreInit;	/*!< non-0 if MPI already initialized when Pilot invoked */
static MPI_Send_func *MPISender;	/*!< function used for PI_Write */

/* Command-line options:
These variables are only meaningful on node 0 (and we assume that only
node 0 can write files).  The resulting service flag settings are broadcast
to other processes to store in their PI_PROCENVT, where they will be checked
during PI_ API calls.  These variables are not used after PI_Configure.
*/
static char *LogFilename;	/*!< Path to log file. NULL = no log file needed. */
static enum {OLP_NONE, OLP_PILOT} OnlineProcess;
enum {OPT_CALLS=0, OPT_DEADLOCK, OPT_JUMPSHOT, OPT_STATS, OPT_TOPO, OPT_TRACE, OPT_END};
static Flag_t Option[OPT_END];	/*!< List of command-line options. 1/0 = flag set/clear */


/*** Public API; each PI_Foo_ is called via PI_Foo wrapper macro ***/

/* Setup functions, to call before StartAll */

/* We don't specify the arg as MPI_Comm because ordinary users would need to #include
 * mpi.h before pilot.h (benchmarkers will be including it because they call MPI_Init).
 * If caller supplies the wrong type, they'll find out soon enough.
 */
void PI_SetCommWorld_( void *comm )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==PREINIT, PI_WRONG_PHASE )

    PI_CommWorld = (MPI_Comm)comm;	// configure with this instead of MPI_COMM_WORLD
}


int PI_Configure_( int *argc, char ***argv )
{
    int i;
    int provided;	// level of thread support provided by MPI
    int procs_avail;    // no. of processes finally available to user
#ifdef PILOT_WITH_MPE
    int save_log_line = PI_CallerLine;  // a Pilot call below will overwrite the global
#endif

    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==PREINIT, PI_WRONG_PHASE )

    thisproc.phase = CONFIG;
    thisproc.start_time = -1.0;

    /* Process command line arguments into LogFilename, OnlineProcess, and
       Option array.  May override caller's setting of PI_CheckLevel.
       This removes any args recognized by Pilot.  MPI_Init will do the same with
       whatever are left.  The user's app can scan anything remaining.  This
       may change *argc and shuffle the array of char* in *argv.  If we're not
       running on node 0, there may be no arguments.  We'll only use the results
       on node 0 below.  (We don't know which node we are since MPI isn't
       initialized yet!)
    */
    ArgvCopy = *argv;		// save a copy for PointerCheck to find bottom of stack
    char *badargs = ParseArgs( argc, argv );	// return needs to be freed

    /* Unless we're in "bench mode" (MPI already initialized), we next init MPI.
       If we need the thread-based online process (because of writing a log
       file), we have to specify the threaded version for node 0.  Unfortunately,
       if we're not node and the arguments _were_ conveyed and parsed, this will
       result in (needlessly) starting the threaded version on ALL nodes; there's
       no way around this.  If the user knows that files can be written from
       non-0 nodes, then the Pilot-based online process can be selected, which
       will avoid starting the threaded version.
    */
    MPI_Initialized( &MPIPreInit );	/* did user already initialize MPI? */
    if ( !MPIPreInit ) {
        MPI_Init_thread( argc, argv, MPI_THREAD_SINGLE, &provided );		/* starts MPI */
    }
    else
        MPI_Query_thread( &provided );

    /* set up handler for any MPI error so Pilot can provide details then abort */
    MPI_Errhandler errhandler;
    MPI_Comm_create_errhandler( (MPI_Comm_errhandler_function*)HandleMPIErrors, &errhandler );
    MPI_Comm_set_errhandler( PI_CommWorld, errhandler ); // inherited by all communicators

    MPI_Comm_rank( PI_CommWorld, &thisproc.rank );	/* get current process id */
    MPI_Comm_size( PI_CommWorld, &thisproc.worldsize );	/* get number of processes */

    /* find out what the max. tag no. can be (15 bits by standard, up to 31) */
    void *tagub;
    int flag;
    MPI_Comm_get_attr( PI_CommWorld, MPI_TAG_UB, &tagub, &flag );
    MPIMaxTag = flag ? *(int*)tagub : 32767;		// was attrib. defined?

    if ( thisproc.rank == 0 ) {
        if ( badargs )
            fprintf( stderr, PI_BORDER "Unrecognized arguments: %s\n", badargs );

/////////////// should following be done in ParseArgs, so worked-over value of
/////////////// OnlineProcess is available for MPI_Init?
        /* go through the options and determine their runtime implications,
           setting appropriate service flags */
        /*
        int anyopts = 0;
        for ( i=0; i<OPT_END; i++ )
            anyopts = anyopts || Option[i];     Don't think we need anyopts now? */

        /* determine which kinds of data must be logged to supply the service */
        thisproc.svc_flag[LOG_TABLES] = (Flag_t)( Option[OPT_CALLS] ||
                                        Option[OPT_STATS] || Option[OPT_TOPO] || Option[OPT_TRACE]);
        thisproc.svc_flag[LOG_CALLS] = (Flag_t)(Option[OPT_CALLS] ||
                                                Option[OPT_TRACE] || Option[OPT_DEADLOCK] );
        thisproc.svc_flag[LOG_STATS] = Option[OPT_STATS];
        if ( Option[OPT_JUMPSHOT] ) {
#ifdef PILOT_WITH_MPE
            thisproc.svc_flag[LOG_MPE] = 1;
#else
            fprintf( stderr, "\n" PI_BORDER
                     "*** Pilot not installed with MPE so 'j' option will not work!" );
            Option[OPT_JUMPSHOT] = 0;
#endif
        }

        /* services needing an online process/thread */
        thisproc.svc_flag[OLP_DEADLOCK] = Option[OPT_DEADLOCK];

        /* Here's the overall explanation on logging logic:
           - Logging is disabled by default.  The user may want to put PI_Log() calls in their
             code but have them essentially be no-ops unless logging is enabled at run time.
             That happens by either using -pisvc to select a log-producing service, or -pilog
             to specify a log file name.
           - Most -pisvc options (c, s, m, t) will give a Pilot-produced text-based .log file.
             The 'j' option will use MPE to store event data in local memory, then collect and
             write a .clog2 file at end of run.  Both types of logging may be active! */

        /* OLP_LOGFILE refers to the local text-format log file written by the
           online process.  Decide whether this will happen based on the -pisvc
           options -or- if -pilog specified a filename -and- 'j' wasn't specified */
        thisproc.svc_flag[OLP_LOGFILE] =
            (Flag_t)( Option[OPT_CALLS] || Option[OPT_STATS] || Option[OPT_TOPO] || Option[OPT_TRACE] ||
                      ( !Option[OPT_JUMPSHOT] && LogFilename ) );

        /* This is the "one stop shop" to find out if logging is enabled, which could be via the Pilot
            log file or via MPE or both. */
        thisproc.svc_flag[LOGGING] =
            (Flag_t)( thisproc.svc_flag[OLP_LOGFILE] || thisproc.svc_flag[LOG_MPE] );

        /* If logging, save the LogFilename.  It may already have been specified
            by -pilog; if not, use "pilot" as default.  This is the base name;
            if user added an extension, take it off now. */
        if ( thisproc.svc_flag[LOGGING] ) {
            if ( LogFilename ) {            // if name provided, look for extension
                char *dot = strrchr( LogFilename, '.' );
                if ( dot == LogFilename ) LogFilename = NULL;   // disregard if only extension
                else if ( dot ) *dot = '\0';     // truncate any extension
            }
            if ( NULL == LogFilename )      // if (still) NULL, apply default
                LogFilename = strdup( "pilot" );
        }

        /* determine whether additional Pilot "online process" is needed
           to support logfile and/or deadlock checking; if so, assign it to
           rank 1 */
        if ( thisproc.svc_flag[OLP_LOGFILE] || Option[OPT_DEADLOCK] ) {
            OnlineProcess = OLP_PILOT;
            thisproc.svc_flag[OLP_RANK] = 1;
        }
        /*else {
            OnlineProcess = OLP_NONE;
            thisproc.svc_flag[OLP_RANK] = 0;
        }*/

        /* TESTING: print summary of args
                for ( i=0; i<OPT_END; i++) printf( " %d", Option[i] );
                printf( "; olp = %d; fname = %s\n", OnlineProcess,
                        LogFilename ? LogFilename : "NULL" );
                for ( i=0; i<SVC_END; i++) printf( " %d", thisproc.svc_flag[i] );
                printf( "\n===Thread support = %d\n", provided );
                exit(1);
        ********************************/

        /* say hello; suppress this via PI_QuietMode */
        LOUD {
            printf( "\n" PI_BORDER "*** %s\n", PI_HELLO );
            printf( PI_BORDER "*** Available MPI processes: %d; tags for channels: %d\n",
                    thisproc.worldsize, MPIMaxTag );
            printf( PI_BORDER "*** Running with error checking at Level %d\n", PI_CheckLevel );

            /* print the options that are in effect */
            printf( PI_BORDER "*** Command-line options:\n" PI_BORDER "***  " );
            if ( Option[OPT_CALLS] ) printf( " Call_log" );
            if ( Option[OPT_JUMPSHOT] ) printf( " Jumpshot_clog2" );
            if ( Option[OPT_STATS] ) printf( " Stats_log" );   // future
            if ( Option[OPT_TOPO] ) printf( " Topology_log" );      // future
            if ( Option[OPT_TRACE] ) printf( " CSP_traces_log" );  // future
            if ( Option[OPT_DEADLOCK] ) printf( " Deadlock_detection" );
            printf( "\n" );
            if ( LogFilename )
                printf( PI_BORDER "*** Logging to file: %s\n", LogFilename );
            if ( OnlineProcess==OLP_PILOT )
                printf( PI_BORDER "*** Online process running as P1\n" );
        }

        /* check to make sure that threading support is available if we need it
           (this code goes back to the never-implemented online process thread,
            but there would be use for it if we implement PI_MAIN-as-master-thread) */
//        PI_ASSERT( , OnlineProcess!=OLP_THREAD || provided==MPI_THREAD_MULTIPLE,
//                                               PI_THREAD_SUPPORT )
    }

    free( badargs );

    /* broadcast the options to everybody, since we can't assume they parsed
       the args for themselves */

    PI_CALLMPI( MPI_Bcast( thisproc.svc_flag, SVC_END, MPI_UNSIGNED_CHAR, PI_MAIN,
                           PI_CommWorld ) )

    /* initialize table of processes */
    thisproc.processes =
        ( PI_PROCESS * ) malloc( sizeof( PI_PROCESS ) * thisproc.worldsize );
    PI_ASSERT( , thisproc.processes, PI_MALLOC_ERROR )

    /* initialize table of process aliases: default is Pn */
    for ( i = 0; i < thisproc.worldsize; i++ ) {
        sprintf( thisproc.processes[i].name, "P%d", i );
    }

    /* initialize table of function pointers */
    for ( i = 0; i < thisproc.worldsize; i++ ) {
        thisproc.processes[i].run = NULL;
    }

    thisproc.channels = NULL;	// grow using realloc on demand

    /* allocate bundle table */
    thisproc.bundles = malloc( sizeof( PI_BUNDLE * ) * PI_MAX_BUNDLES );
    PI_ASSERT( , thisproc.bundles, PI_MALLOC_ERROR )

    for ( i = 0; i < PI_MAX_BUNDLES; i++ ) {
        thisproc.bundles[i] = NULL;
    }

    /* initialize process, channel, bundle counts */
    thisproc.allocated_processes = 0;
    thisproc.allocated_channels = 0;
    thisproc.allocated_bundles = 0;

    /* create a place holder for rank 0 and set name */
    PI_SetName( PI_CreateProcess_( NULL, 0, NULL, 0 ), "main" );

    /* Determine which MPI function will do PI_Write; default is MPI_Send,
       which is often buffered, but could be unbuffered in some circumstances.
       Deadlock detection can't live with that uncertainty, so use MPI_Ssend
       if it's enabled, which will act unbuffered and unf'ly add overhead.
    */
    if ( Option[OPT_DEADLOCK] ) MPISender = (MPI_Send_func *)MPI_Ssend;
    else MPISender = (MPI_Send_func *)MPI_Send;

    /* If we need to start an online process, create it now, so it gets
       rank 1; this will abort if there aren't at least 2 MPI processes
       available.  Filename has to be sent from PI_MAIN in PI_StartAll, since
       P1 doesn't know it.
    */
    procs_avail = thisproc.worldsize;
    if ( thisproc.svc_flag[OLP_RANK] == 1 ) {
        procs_avail--;	/* decr. worldsize since OLP absorbs 1 */
        PI_SetName( PI_CreateProcess_( OnlineProcessFunc, 0, NULL, 0 ),
                    "Pilot Online Process" );
    }

    /* MPE setup calls */

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        MPE_Init_log();

        /* At this point, only PI_MAIN processed command line args so only it knows the
           log file name.  Broadcast it to all processes, since they'll need if for
           MPE_Finish_log at end of execution.  Send the buffer length first, then the string. */
        int namelen;
        if ( thisproc.rank == 0 ) namelen = strlen( LogFilename ) + 1;
        PI_CALLMPI( MPI_Bcast( &namelen, 1, MPI_INT, PI_MAIN, PI_CommWorld ) )

        if ( thisproc.rank != 0 ) {
            LogFilename = malloc( namelen );
            PI_ASSERT( , LogFilename, PI_MALLOC_ERROR )
        }

        PI_CALLMPI( MPI_Bcast( LogFilename, namelen, MPI_CHAR, PI_MAIN, PI_CommWorld ) )

        /* The following cases set up format strings for each loggable state and solo event.  It's commented
           in such detail because MPE's rules are underexplained in the scanty/old documentation:
           1) strlen(format string) <= 40 bytes (NUL terminator doesn't count). This is the place to put
              literal text; don't add it through MPE_Log_pack where it will consume the precious 32-byte buffer!
           2) At log viewing time, Jumpshot will fetch the format string and unpack corresponding binary data
              from the buffer packed by MPE_Log_pack, which is 32 bytes max.  Each item of binary data takes
              only the same space as its C datatype, except that C-strings are stored without NUL terminator but
              preceded by a 2-byte binary string length.  To be clear:  There is ONE copy of the format string in
              Rank 0 that gets recorded in the logfile, and there are MULTIPLE instances of 32-byte data
              buffers, one for each logged state or event.  These only get combined by Jumpshot.  All ranks
              need (the same) event IDs, but only Rank 0 needs the corresponding event descriptions.  The format
              strings do not get used until Jumpshot is working.  To put it another way, if you consider
              fprint( "format", a, b, c ), then "format" is set (once) via MPE_Describe_info_state, and the
              data values for each log call -- each instance of {a, b, c} -- are encoded via MPE_Log_Pack and
              recorded by MPE_Log_event.
           3) For documentation, each format string is given below along with the total no. of bytes consumed
              by all its packed data items EXCEPT FOR STRINGS.  So, "X %d %d" is 8 (2*4-byte int) but
              "X %s %d" is 6 (2-byte string length + 4-byte int) PLUS strlen(string).  So you can figure
              out the "budget" for the variable length string from 32 minus the number given here.  The budget
              applies when you call MPE_Log_pack, and you can easily truncate an overlong string by setting the
              4th arg to min(strlen(string),budgeted length).  The penalty for exceeding the budgeted length is
              that MPE_Log_pack will return MPE_LOG_PACK_FAIL on this pack or a subsequent one, then Jumpshot will
              find garbage data in that position and likely crash when the user right-clicks the graphical object.
              Instead of checking for MPE_LOG_PACK_FAIL (then how do you report it, and is it fatal?), we avoid it.
              The penalty for letting an underbudget short string sneak through without limiting it to strlen is
              that its NUL terminator and following garbage will be packed and later displayed by Jumpshot.
           4) The dodgy situation is where there are multiple %s strings in one format.  Then you have to split out
              a budget for each one.  You can't just calculate it on the fly from 32 minus this number.
           5) The string "budgets" are #defined here as LOG_xxx_SMAX and referred to in code that logs events.
              We could make a macro to calculate the budget given the consumed bytes, but it's better for the
              maintainer to see the budget number, so we leave the (trivial) manual calculation.
           6) WARNING:  There is a bug, probably in Jumpshot, whereby when you START a format with a % token and
              continue with any other text/tokens, the data substitutions will be in the wrong places and the
              display will be garbled!  This doesn't apply to a single token like "%d", but "%d lines" would not
              work, you would need something like "Lines: %d".  The only known (by us) workaround is to start such
              formats with some/any literal text, then the output will be fine.  Since we use MPE and Jumpshot
              "as is" we didn't try to find the location of this bug and fix it.
           7) NOTES:  The "40 byte" and "32 byte" limits represent fixed field/record lengths in the CLOG-2 file
              format.  MPE just respects those limits.  MPE_Log_pack does not know the format string!  Therefore,
              you must call MPE_Log_pack in the same sequence matching the format string.  The sole use it makes
              of the tokentype is to determine how many binary bytes to memcpy from the data pointer.
              It does not use the tokentype to do any conversions (those are done by Jumpshot at viewing time).
           8) CAUTION:  For "states" both the start and end are logged.  You can attach info data to EITHER call,
              but not both.  If you try both, the end data overwrites the start data.
           9) MPE "deprecations":  We are calling MPE_Describe_info_state and MPE_Log_event, which are part of the
              older API, instead of "urged" Describe_comm_state and Log_comm_event.  Since we don't mess with threads
              and communicators, the simpler interface is superior for our purposes.  Furthermore, in MPE_Log_event
              the "data" argument is always zero because it became obsolete after Describe_info_state was invented.

           Your "one stop shop" for convenient MPE (also MPI) API documentation is here:
           http://www.physics.drexel.edu/~valliere/PHYS405/www/

           Based on the above, advice for maintainers:
           1) Start by laying out the *format* you want ideally.  See MPE_Describe_info_state for supported
              printf-style tokens.
           2) If this exceeds 40 bytes, cut back on literal text.
           3) Next, make sure the encoded variable data fits in 32 bytes.  MPE_Describe_info_state and MPE_Log_pack
              give the encoded lengths. Here you're ONLY looking at the packed size of C datatypes.  If you have %s
              variable length strings, determine a budget for each one and make sure you truncate it when packing.
           4) Don't get confused and reserve space for the format string's literals in this buffer: they won't ever
              be there, they're stored separately (one time)!
        */
// *INDENT-OFF*
        for (i=LOG_WRITE; i<LOG_LAST; i=i+1)             // initialize state events
        {
            MPE_Log_get_state_eventIDs( &thisproc.mpe_eventse[i][0], &thisproc.mpe_eventse[i][1] );
            if ( thisproc.rank == 0 ) {
                switch(i) {
                case LOG_CONFIGURE:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_CONFIGURE][0], thisproc.mpe_eventse[LOG_CONFIGURE][1],
                                             "PI_Configure", LOG_CLR_CONFIG, "Line No. %d, Returned %d" );      // used 8
                    break;
                case LOG_WRITE:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_WRITE][0], thisproc.mpe_eventse[LOG_WRITE][1],
                                             "PI_Write", LOG_CLR_OUTPUT, "Line No. %d, %s(%d)" );               // %s max 32-10=22
                    #define LOG_WRITE_SMAX 22
                    break;
                case LOG_READ:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_READ][0], thisproc.mpe_eventse[LOG_READ][1],
                                             "PI_Read", LOG_CLR_INPUT, "Line No. %d, %s(%d)" );                 // %s max 32-10=22
                    #define LOG_READ_SMAX 22
                    break;
                case LOG_COMPUTE:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_COMPUTE][0], thisproc.mpe_eventse[LOG_COMPUTE][1],
                                             "Compute", LOG_CLR_COMPUTE, "Starts at Line No. %d, %s(%d)" );     // %s max 32-10=22
                    #define LOG_COMPUTE_SMAX 22
                    break;
                case LOG_SELECT:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_SELECT][0], thisproc.mpe_eventse[LOG_SELECT][1],
                                             "PI_Select", LOG_CLR_COLL_INP, "Line No. %d, %s(%d), Bundle %s, Retd %d" ); // %s max 32-16=16, 8 each
                    #define LOG_SELECT_S1MAX 8
                    #define LOG_SELECT_S2MAX 8
                    break;
                case LOG_BROADCAST:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_BROADCAST][0], thisproc.mpe_eventse[LOG_BROADCAST][1],
                                             "PI_Broadcast", LOG_CLR_COLL_OUT, "Line No. %d, %s(%d), Bundle %s" ); // %s max 32-12=20, 10 each
                    #define LOG_BROADCAST_S1MAX 10
                    #define LOG_BROADCAST_S2MAX 10
                    break;
                case LOG_SCATTER:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_SCATTER][0], thisproc.mpe_eventse[LOG_SCATTER][1],
                                             "PI_Scatter", LOG_CLR_COLL_OUT, "Line No. %d, %s(%d), Bundle %s" ); // %s max 32-12=20, 10 each
                    #define LOG_SCATTER_S1MAX 10
                    #define LOG_SCATTER_S2MAX 10
                    break;
                case LOG_REDUCE:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_REDUCE][0], thisproc.mpe_eventse[LOG_REDUCE][1],
                                             "PI_Reduce", LOG_CLR_COLL_INP, "Line No. %d, %s(%d), Bundle %s" ); // %s max 32-12=20, 10 each
                    #define LOG_REDUCE_S1MAX 10
                    #define LOG_REDUCE_S2MAX 10
                    break;
                case LOG_GATHER:
                    MPE_Describe_info_state( thisproc.mpe_eventse[LOG_GATHER][0], thisproc.mpe_eventse[LOG_GATHER][1],
                                             "PI_Gather", LOG_CLR_COLL_INP, "Line No. %d, %s(%d), Bundle %s" ); // %s max 32-12=20, 10 each
                    #define LOG_GATHER_S1MAX 10
                    #define LOG_GATHER_S2MAX 10
                    break;
                }
            }
        }

        for (i=LOG_WRITE; i<LOG_LAST; i=i+1)              // initialize solo events
        {
            MPE_Log_get_solo_eventID( &thisproc.mpe_event[i] );
            if ( thisproc.rank == 0 ) {
                switch(i) {
                case LOG_WRITE_MSG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_WRITE_MSG],
                                             "Message Passing Data", LOG_CLR_STATE_EVT, "[Quantity] First Value: %s" ); // %s max 32-2=30
                    #define LOG_WRITE_MSG_SMAX 30
                    break;
                case LOG_CHANNEL:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_CHANNEL],
                                             "Message Passing Channel", LOG_CLR_STATE_EVT, "Channel %s" );      // %s max 32-2=30
                    #define LOG_CHANNEL_SMAX 30
                    break;
                case LOG_BROADCAST_MSG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_BROADCAST_MSG],
                                             "Broadcast Data", LOG_CLR_STATE_EVT, "[Quantity] First Value: %s" ); // %s max 32-2=30
                    #define LOG_BROADCAST_MSG_SMAX 30
                    break;
                case LOG_SCATTER_MSG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_SCATTER_MSG],
                                             "Scatter Data", LOG_CLR_STATE_EVT, "[Quantity] First Value: %s" ); // %s max 32-2=30
                    #define LOG_SCATTER_MSG_SMAX 30
                    break;
                case LOG_REDUCE_MSG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_REDUCE_MSG],
                                             "Reduce Data", LOG_CLR_STATE_EVT, "[Quantity] First Value: %s" );  // %s max 32-2=30
                    #define LOG_REDUCE_MSG_SMAX 30
                    break;
                case LOG_GATHER_MSG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_GATHER_MSG],
                                             "Gather Data", LOG_CLR_STATE_EVT, "[Quantity] First Value: %s" );  // %s max 32-2=30
                    #define LOG_GATHER_MSG_SMAX 30
                    break;
                case LOG_TRYSELECT:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_TRYSELECT],                                 // %s max 32-10=20
                                             "PI_TrySelect Result", LOG_CLR_NONSTATE_EVT, "Line No. %d, Bundle %s, Returned %d" );
                    #define LOG_TRYSELECT_SMAX 20
                    break;
                case LOG_CHANNELHASDATA:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_CHANNELHASDATA],                            // %s max 32-10=20
                                             "PI_ChannelHasData Result", LOG_CLR_NONSTATE_EVT, "Line No. %d, Channel %s, Returned %d" );
                    #define LOG_CHANNELHASDATA_SMAX 20
                    break;
                case LOG_LOG:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_LOG],
                                             "PI_Log Content", LOG_CLR_NONSTATE_EVT, "Line No. %d: %s" );       // %s max 32-6=26
                    #define LOG_LOG_SMAX 26
                    break;
                case LOG_STARTTIME:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_STARTTIME],
                                             "PI_StartTime", LOG_CLR_NONSTATE_EVT, "Line No. %d" );             // used 4
                    break;
                case LOG_ENDTIME:
                    MPE_Describe_info_event( thisproc.mpe_event[LOG_ENDTIME],
                                             "PI_EndTime", LOG_CLR_NONSTATE_EVT, "Line No. %d, Elapsed Time %E Sec." ); // used 12
                    break;
                }
            }
        }
// *INDENT-ON*
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &save_log_line );        // where PI_Configure was called
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &procs_avail );          // return value of PI_Configure
        MPE_Log_event( thisproc.mpe_eventse[LOG_CONFIGURE][0], 0, bytebuf );  // mark start of configuration phase
    }
#endif

    return procs_avail;	/* procs available to user, including PI_MAIN */
}

/* Note: Process IDs (=MPI rank) run from 0
   lang switch: 0=C (call by value), 1=Fortran (call by loc) */
PI_PROCESS *PI_CreateProcess_( PI_WORK_FUNC f, int index, void *opt_pointer, int lang )
{
    PI_ON_ERROR_RETURN( NULL )
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )

    /* assign the new process the next available MPI process rank */
    int r = thisproc.allocated_processes++;
    PI_ASSERT( , r<thisproc.worldsize, PI_INSUFFICIENT_MPIPROCS )

    /* must supply function unless it's the zero main process */
    PI_ASSERT( , r==0 || f!=NULL, PI_NULL_FUNCTION )
    thisproc.processes[r].run = f;
    thisproc.processes[r].call = lang;

    snprintf( thisproc.processes[r].name, PI_MAX_NAMELEN, "P%d", r ); // default name "Pn"
    thisproc.processes[r].argument = index;
    thisproc.processes[r].argument2 = opt_pointer;
    thisproc.processes[r].rank = r;
    thisproc.processes[r].magic = PI_PROC;
    return &thisproc.processes[r];
}

/* Note: Channel tags run from 1, with 0 reserved for special use, i.e.,
   log messages */
PI_CHANNEL *PI_CreateChannel_( PI_PROCESS *from, PI_PROCESS *to )
{
    PI_ON_ERROR_RETURN( NULL )
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )

    int f, t;		// MPI ranks of from/to processes

    if ( from == NULL ) {
        f = PI_MAIN;
    } else {
        PI_ASSERT( LEVEL(1), ISVALID(PI_PROC,from), PI_INVALID_OBJ )
        f = from->rank;
    }

    if ( to == NULL ) {
        t = PI_MAIN;
    } else {
        PI_ASSERT( LEVEL(1), ISVALID(PI_PROC,to), PI_INVALID_OBJ )
        t = to->rank;
    }

    PI_ASSERT( , t!=f, PI_ENDPOINT_DUPLICATE )

    PI_CHANNEL *pc = malloc( sizeof( PI_CHANNEL ) );
    PI_ASSERT( , pc, PI_MALLOC_ERROR )

    // expand array of PI_CHANNEL* pointers
    thisproc.channels = realloc( thisproc.channels,
                                 (1+thisproc.allocated_channels)*sizeof(PI_CHANNEL*) );
    PI_ASSERT( , thisproc.channels, PI_MALLOC_ERROR )
    thisproc.channels[thisproc.allocated_channels] = pc;

    /* channel ID & initial tag is just 1+no. allocated so far */
    pc->chan_id = pc->chan_tag = ++thisproc.allocated_channels;

    /* it's unlikely that the user will blow past this limit, but better check */
    PI_ASSERT( , pc->chan_tag<MPIMaxTag, PI_MAX_TAGS )

    pc->producer = f;
    pc->consumer = t;

    snprintf( pc->name, PI_MAX_NAMELEN,
              "C%d", pc->chan_id ); 	// default name "Cn"

    pc->bundle = NULL;		/* initially not part of bundle */
    pc->magic = PI_CHAN;

    return pc;
}

PI_BUNDLE *PI_CreateBundle_( enum PI_BUNUSE usage, PI_CHANNEL *const array[], int size )
{
    PI_ON_ERROR_RETURN( NULL )	/* makes caller func return NULL to user */
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )
    PI_ASSERT( , array, PI_NULL_CHANNEL )
    PI_ASSERT( , size>0, PI_ZERO_MEMBERS )
    PI_ASSERT( , thisproc.allocated_bundles<PI_MAX_BUNDLES, PI_MAX_BUNDLES )

    PI_BUNDLE *b = malloc( sizeof( PI_BUNDLE ) );
    PI_ASSERT( , b, PI_MALLOC_ERROR )

    /* Depending on the bundle usage, we'll extract some properties from the
       first channel and propagate them to the others in the bundle:
       - all have a common endpoint, either the producer or consumer
       - a Selector has a common tag; collective bundles don't use tags
    */
    PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,array[0]), PI_INVALID_OBJ )

    int commonEnd = (usage==PI_BROADCAST || usage==PI_SCATTER)
                    ? array[0]->producer : array[0]->consumer;
    int commonTag = usage==PI_SELECT ? array[0]->chan_tag : 0;

    b->usage = usage;
    b->size = size;
    b->channels = malloc( sizeof( PI_CHANNEL * ) * size );
    PI_ASSERT( , b->channels, PI_MALLOC_ERROR )

    /* copy array of channels into bundle, checking/setting properties */
    int i, j;
    for ( i = 0; i < size; i++ ) {
        PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,array[i]), PI_INVALID_OBJ )

        /* verify that channel is not already part of a bundle:
           - NULL and chan_tag are the initial values
        */
        PI_ASSERT( ,
                   NULL==array[i]->bundle && array[i]->chan_id==array[i]->chan_tag,
                   PI_BUNDLE_ALREADY )

        /* verify that each member channel has the required common end */
        switch ( usage ) {
        case PI_SELECT:
        case PI_GATHER:
        case PI_REDUCE:
            PI_ASSERT( , array[i]->consumer==commonEnd, PI_BUNDLE_READEND )
            break;

        case PI_BROADCAST:
        case PI_SCATTER:
            PI_ASSERT( , array[i]->producer==commonEnd, PI_BUNDLE_WRITEEND )
            break;
        }

        /* verify that there are no duplicate processes on rim */
        for ( j = 0; j < i; j++ ) {
            if ( usage==PI_BROADCAST || usage==PI_SCATTER ) {
                PI_ASSERT( , array[i]->consumer!=array[j]->consumer, PI_BUNDLE_DUPLICATE )
            } else {
                PI_ASSERT( , array[i]->producer!=array[j]->producer, PI_BUNDLE_DUPLICATE )
            }
        }

        /* propagate common tag for Selector bundle */
        if ( usage == PI_SELECT )
            array[i]->chan_tag = commonTag;
        /* make each member of collective channel point to this bundle */
        else
            array[i]->bundle = b;

        b->channels[i] = array[i];	// store the channel member in bundle
    }

    b->narrow_end = (usage==PI_BROADCAST || usage==PI_SCATTER) ? FROM : TO;

    if ( usage == PI_SELECT ) {
        b->comm = PI_CommWorld;
    } else {
        /* create the communicator */
        MPI_Group world, group;

        /* get handle on WORLD comm. group */
        PI_CALLMPI( MPI_Comm_group( PI_CommWorld, &world ) )

        int *ranks = malloc( sizeof( int ) * ( b->size + 1 ) );
        PI_ASSERT( , ranks, PI_MALLOC_ERROR )

        /* fill in ranks array for new group; bundle base goes in rank 0 */
        if ( usage==PI_BROADCAST || usage==PI_SCATTER ) {
            ranks[0] = b->channels[0]->producer;
            for ( i = 1; i < ( b->size + 1 ); i++ )
                ranks[i] = b->channels[i-1]->consumer;
        } else {	/* GATHER or REDUCE */
            ranks[0] = b->channels[0]->consumer;
            for ( i = 1; i < ( b->size + 1 ); i++ )
                ranks[i] = b->channels[i-1]->producer;
        }

        if ( usage==PI_REDUCE ) {
            /* For reduce, we leave the consumer out of the communicator,
               because it cannot call MPI_Reduce without providing data to
               the reduce operation -- and it doesn't have any!  So we let
               just the producer ranks participate in MPI_Reduce.  The rank at
               channels[0]->producer will be responsible for sending the
               reduced result to channel[0]->consumer in a separate message.
                */
            PI_CALLMPI( MPI_Group_incl( world, b->size, &ranks[1], &group ) )
        }
        else PI_CALLMPI( MPI_Group_incl( world, b->size + 1, ranks, &group ) )
            free( ranks );

        /* Note: All the members of the "world" are supposed to call this, even
         * if they're not members of the new group.  This will happen because
         * all processes execute all the configuration statements.  Outsiders
         * will get a value of MPI_COMM_NULL, but that's fine because they
         * would not be able to utilize this bundle anyway.  However, StopMain
         * must not later try to call Comm_free for this bundle in this process,
         * because its comm member is invalid.
         */
        PI_CALLMPI( MPI_Comm_create( PI_CommWorld, group, &( b->comm ) ) )
    }

    b->magic = PI_BUND;
    thisproc.bundles[thisproc.allocated_bundles] = b;

    /* bundle ID is just 1+no. allocated so far */
    b->bund_id = ++thisproc.allocated_bundles;

    snprintf( b->name, PI_MAX_NAMELEN,
              "B%d@P%d", b->bund_id, commonEnd );	// default name "Bn@Pc"

    return b;
}

PI_CHANNEL **PI_CopyChannels_( enum PI_COPYDIR direction, PI_CHANNEL *const array[], int size )
{
    PI_ON_ERROR_RETURN(NULL)
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )
    PI_ASSERT( , array, PI_NULL_CHANNEL )
    PI_ASSERT( , size>0, PI_ZERO_MEMBERS )

    PI_CHANNEL **newArray = malloc( sizeof( PI_CHANNEL * ) * size );
    PI_ASSERT( , newArray, PI_MALLOC_ERROR )

    /* create channels with same or reversed endpoints */
    int i;
    PI_PROCESS *from, *to;
    for ( i = 0; i < size; i++ ) {
        PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,array[i]), PI_INVALID_OBJ )

        from = &thisproc.processes[array[i]->producer];
        to = &thisproc.processes[array[i]->consumer];

        newArray[i] = direction==PI_SAME ?
                      PI_CreateChannel_( from, to ) :
                      PI_CreateChannel_( to, from );
    }
    return newArray;
}

void PI_SetName_( void *object, const char *name )
{
    /* There is a reason we allow GetName in either phase (and with a NULL arg),
       but only allow SetName in the Config phase:

       If we allowed SetName in the Exec phase, then the same object could have
       different names in different processes.  Assuming that there was some use
       for this, at least it would make a call log confusing.  By restricting
       SetName to the Config phase, this forces all processes to have the same
       name for any given object.  And, since it's called in the Config phase,
       the caller certainly has the necessary object pointer.  In contrast,
       calling GetName(NULL) in the Exec phase is the easy way to find out one's
       process name in the absence of a PI_PROCESS* pointer.
    */
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )
    PI_ASSERT( , object, PI_INVALID_OBJ )

    char *nameField;	 	// points to the object's name array

    /* probe magic number to identify object type */

    if ( ISVALID(PI_PROC,(PI_PROCESS*)object) ) {
        nameField = ((PI_PROCESS*)object)->name;
    }
    else if ( ISVALID(PI_CHAN,(PI_CHANNEL*)object) ) {
        nameField = ((PI_CHANNEL*)object)->name;
    }
    else if ( ISVALID(PI_BUND,(PI_BUNDLE*)object) ) {
        nameField = ((PI_BUNDLE*)object)->name;
    }
    else {
        PI_ASSERT( , 0, PI_INVALID_OBJ )	// can't identify it ->  abort
        return;				// to make compiler not warn
    }

    if ( name != NULL )
        strncpy( nameField, name, PI_MAX_NAMELEN );	// make a copy
    else
        strcpy( nameField, "" );	// use empty string if NULL
}

int PI_StartAll_( void )
{
    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==CONFIG, PI_WRONG_PHASE )

    thisproc.phase = RUNNING;

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_event( thisproc.mpe_eventse[LOG_CONFIGURE][1], 0, NULL );  // mark end of configuration phase

        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        if ( thisproc.rank == 0 || thisproc.processes[thisproc.rank].run ) {    // was process "created"?
            int namelen = strlen( thisproc.processes[thisproc.rank].name );     // it's got a name(arg) now
            MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_COMPUTE_SMAX,namelen), thisproc.processes[thisproc.rank].name );
            MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        }
        else {                                  // this happens if no. MPI processes > no. of PI_CreateProcess
            char *idle = "Idle Process";
            int namelen = strlen( idle ),
                zero = 0;
            MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_COMPUTE_SMAX,namelen), idle );
            MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &zero );
        }
        MPE_Log_event( thisproc.mpe_eventse[LOG_COMPUTE][0], 0, bytebuf ); // start of execution phase (Compute state)
    }
#endif

    if ( thisproc.rank == 0 ) {

        LOUD printf( PI_BORDER
                     "*** Allocated Pilot processes: %d; channels: %d; bundles: %d\n",
                     thisproc.allocated_processes, thisproc.allocated_channels,
                     thisproc.allocated_bundles );
        int spare = thisproc.worldsize - thisproc.allocated_processes;
        if ( spare > 0 )
            LOUD printf( PI_BORDER
                         "*** Note that --%d-- MPI processes will be idle!\n", spare );
        LOUD printf( "\n\n" );

        /* synchronize on barrier below, now we're done printing */
        MPI_Barrier( PI_CommWorld );  //// matches barrier below ////

        /* If there is a log file, we have to explicitly send the filename
           length and string.  The online process/thread will be waiting to
           receive this.  (An online thread could get it out of this node's
           address space, but we still send it for consistency.)
        */
        if ( OnlineProcess != OLP_NONE ) {
            int flen = 0;               // assume there won't be a filename
            if ( thisproc.svc_flag[OLP_LOGFILE] ) flen = strlen(LogFilename)+1;
            PI_CALLMPI( MPI_Send( &flen, 1, MPI_INT,
                                  thisproc.svc_flag[OLP_RANK], 0, PI_CommWorld ) )
            if ( flen ) {
                PI_CALLMPI( MPI_Send( LogFilename, flen, MPI_CHAR,
                                      thisproc.svc_flag[OLP_RANK], 0, PI_CommWorld ) )
            }
        }

        return 0;
        /* continues executing main(), the master process */
    }

    MPI_Barrier( PI_CommWorld );  //// matches barrier above ////

    PI_PROCESS *p = &thisproc.processes[thisproc.rank];
    int status = 0;

    if ( p->run ) {
        /* execute function associated with allocated process */

        if ( p->call == 0 )	// C-style call by value
            status = p->run( p->argument, p->argument2 );

        else 			// Fortran-style call by ref
            status = ((PI_WORK_FTN)p->run)( &p->argument, &p->argument2 );

    }
    /* if not allocated a process (run==NULL), just fall through */

    /* since we're not the main process, this will normally call exit */
    PI_StopMain_( status );

    /* if we got here, we're a non-main process and Pilot is in "bench
       mode", so return our MPI rank and let the user decide what to do next

       NOTE: now that we have the status arg, should return it instead of rank?
    */
    return thisproc.rank;
}


/* Functions that can be called during configuration or after StartAll */

void PI_StartTime_( void )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==CONFIG || thisproc.phase==RUNNING, PI_WRONG_PHASE )

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        MPE_Log_event( thisproc.mpe_event[LOG_STARTTIME], 0, bytebuf );
    }
#endif

    thisproc.start_time = MPI_Wtime( );
}

double PI_EndTime_( void )
{
    double now = MPI_Wtime( );
    double elapsed_time;

    PI_ON_ERROR_RETURN( 0.0 )
    PI_ASSERT( , thisproc.phase==CONFIG || thisproc.phase==RUNNING, PI_WRONG_PHASE )

    if ( thisproc.start_time > 0.0 ) {
        elapsed_time = now - thisproc.start_time;

#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {
            bytebuf_pos = 0;
            MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
            MPE_Log_pack( bytebuf, &bytebuf_pos, 'E', 1, &elapsed_time );
            MPE_Log_event( thisproc.mpe_event[LOG_ENDTIME], 0, bytebuf );
        }
#endif
        return ( elapsed_time );
    }

    fprintf( stderr, "\n" PI_BORDER
             "PI_EndTime: Timing not initialized.  Call PI_StartTime() first.\n" );
    return 0.0;
}

int PI_IsLogging_()
{
    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==CONFIG || thisproc.phase==RUNNING, PI_WRONG_PHASE )

    return thisproc.svc_flag[LOGGING];
}

const char *PI_GetName_(const void *object)
{
    static const char *noname = "Configuration Phase";

    PI_ON_ERROR_RETURN( NULL )
    PI_ASSERT( , thisproc.phase==CONFIG || thisproc.phase==RUNNING, PI_WRONG_PHASE )

    /* special case NULL object -> name of this process */

    if ( object == NULL ) {
        if ( thisproc.phase==RUNNING )
            return thisproc.processes[thisproc.rank].name;
        else
            return noname;
    }

    /* probe magic number to identify object type */

    if ( ISVALID(PI_PROC,(PI_PROCESS*)object) )
        return ((PI_PROCESS*)object)->name;

    if ( ISVALID(PI_CHAN,(PI_CHANNEL*)object) )
        return ((PI_CHANNEL*)object)->name;

    if ( ISVALID(PI_BUND,(PI_BUNDLE*)object) )
        return ((PI_BUNDLE*)object)->name;

    PI_ASSERT( , 0, PI_INVALID_OBJ )	// can't identify it -> abort
    return NULL;	// to make compiler not warn
}


/* Functions to call only after StartAll */

void PI_Write_( PI_CHANNEL *c, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , c, PI_NULL_CHANNEL )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,c), PI_INVALID_OBJ )
    PI_ASSERT( , c->producer==thisproc.rank, PI_ENDPOINT_WRITER )

    int i;
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];

    PI_BUNDLE *b = c->bundle;	// collective bundle associated with channel
    if ( b ) {			// NULL if point-to-point

        /* make sure we're on the rim of the bundled channel */
        PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_SYSTEM_ERROR )
        PI_ASSERT( , b->narrow_end==TO, PI_BUNDLED_CHANNEL )
    }

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_VALS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_WRITE][0], 0, NULL ); // mark start of PI_Write
#endif

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
    LOGCALL( "Wri", c->chan_id, format, 1, mpiArgCount, mpiArgs )

    /* Calculate format signature; if channel write, send to reader for matchup;
     * if bundle "write" (to Gather or Reduce) receive format from "narrow" end
     * of bundle and compare. If we're in a reducer bundle AND we're "rank 0"
     * in the communicator, responsible to collect the result and send it to the
     * "narrow" end, then we do both operations, since the PI_Reduce process is
     * not in the communicator and cannot broadcast its format to the writers.
     */
    if ( PI_CheckLevel >= 2 ) {
        int buff,
            sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        if ( b==NULL || (b->usage==PI_REDUCE && c==b->channels[0]) ) {
            PI_CALLMPI( MPISender( &sig, 1, MPI_INT, c->consumer,
                                   c->chan_tag, PI_CommWorld ) )
        }

        if ( b ) {
            if ( b->usage==PI_REDUCE && c==b->channels[0] ) {
                // We're rank 0 in reducer bundle, send signature
                PI_CALLMPI( MPI_Bcast( &sig, 1, MPI_INT,	// what we're sending
                                       0, b->comm ) )	// "root" is rank 0 in bundle
            }
            else {		// all other bundle cases (we're on the rim)
                // Get signature from "root" and compare to ours
                PI_CALLMPI( MPI_Bcast( &buff, 1, MPI_INT,	// what we're receiving
                                       0, b->comm ) )
                PI_ASSERT( LEVEL(2), buff==sig, PI_FORMAT_MISMATCH )
            }
        }
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* Log each item */
        if ( i>0 ) LOGCALL( "Wri", c->chan_id, format, i+1, mpiArgCount, arg );

        if ( b==NULL ) {

            /* Reduce operation is not valid outside of reducer bundle */
            PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

#ifdef PILOT_WITH_MPE
            if ( thisproc.svc_flag[LOG_MPE] ) {
                mybuff[0] = '\0';
                bytebuf_pos = 0;
                // skip leading PI_LOGSEP char output by interpArg
                int namelen = strlen( interpArg(mybuff, LOG_WRITE_MSG_SMAX, "Wri", &mpiArgs[i]) ) - 1;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_WRITE_MSG_SMAX,namelen) , mybuff+1 );
                MPE_Log_event( thisproc.mpe_event[LOG_WRITE_MSG], 0, bytebuf ); // event bubble in PI_Write
                MPE_Log_send( c->consumer, c->chan_tag, arg->count );   // sender's end of message arrow
            }
#endif

            PI_CALLMPI( MPISender( arg->buf, arg->count, arg->type, c->consumer,
                                   c->chan_tag, PI_CommWorld ) )
        }
        else if ( b->usage==PI_GATHER ) {

            /* Reduce operation is not valid outside of reducer bundle */
            PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

            /* MPI_Gatherv here sends data to consumer process within comm
               communicator (dedicated to this bundle).  In PI_Gather, the
               same MPI_Gatherv receives the data. */

#ifdef PILOT_WITH_MPE
            if ( thisproc.svc_flag[LOG_MPE] ) {
                mybuff[0] = '\0';
                bytebuf_pos = 0;
                // skip leading PI_LOGSEP char output by interpArg
                int namelen = strlen( interpArg(mybuff, LOG_GATHER_MSG_SMAX, "Wri", &mpiArgs[i]) ) - 1;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_GATHER_MSG_SMAX,namelen), mybuff+1 );
                MPE_Log_event( thisproc.mpe_event[LOG_GATHER_MSG], 0, bytebuf );    // event bubble in PI_Write
                MPE_Log_send( c->consumer, c->chan_tag, arg->count );   // sender's end of message arrow
            }
#endif

            PI_CALLMPI( MPI_Gatherv(
                            arg->buf, arg->count, arg->type, // what we're sending
                            NULL, NULL, NULL, 0,	// ignored on sender call
                            0, b->comm ) )	// "root" is rank 0 in communicator
        }
        else {	// must be PI_REDUCE

            PI_ASSERT( , arg->op!=MPI_OP_NULL, PI_OP_MISSING )

            /* First, all members of the communicator participate in the
            reduction by calling MPI_Reduce. We need a result buffer for this
            if we are rank 0 in the communicator, i.e., the first channel
            in the bundle.
            */
            MPI_Aint lb, extent;
            void *resultbuf;

            if ( c==b->channels[0] ) {
                PI_CALLMPI( MPI_Type_get_extent( arg->type, &lb, &extent ) )
                resultbuf = malloc( extent * arg->count );
                PI_ASSERT( , resultbuf, PI_MALLOC_ERROR )

            }
            else resultbuf = NULL;

#ifdef PILOT_WITH_MPE
            if ( thisproc.svc_flag[LOG_MPE] ) {
                mybuff[0] = '\0';
                bytebuf_pos = 0;
                // skip leading PI_LOGSEP char output by interpArg
                int namelen = strlen( interpArg(mybuff, LOG_REDUCE_MSG_SMAX, "Wri", &mpiArgs[i]) ) - 1;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_REDUCE_MSG_SMAX,namelen), mybuff+1 );
                MPE_Log_event( thisproc.mpe_event[LOG_REDUCE_MSG], 0, bytebuf );    // event bubble in PI_Write
                MPE_Log_send( c->consumer, c->chan_tag, arg->count );       // sender's end of message arrow
            }
#endif

            PI_CALLMPI( MPI_Reduce(
                            arg->buf,			// our contribution
                            resultbuf,			// result here if we're "root"
                            arg->count, arg->type,	// what we're sending
                            arg->op,			// the reduce operation
                            0, b->comm ) )	    // "root" is rank 0 in communicator

            /* Next, if our channel is first in the bundle, we have the
               task of sending the result to the process at the bundle's base.
                */
            if ( resultbuf ) {

                PI_CALLMPI( MPISender( resultbuf, arg->count, arg->type, c->consumer,
                                       c->chan_tag, PI_CommWorld ) )
                free( resultbuf );
            }
        }
    }

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen( thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_WRITE_SMAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        MPE_Log_event( thisproc.mpe_eventse[LOG_WRITE][1], 0, bytebuf ); // mark end of PI_Write
    }
#endif
}

void PI_Read_( PI_CHANNEL *c, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , c, PI_NULL_CHANNEL )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,c), PI_INVALID_OBJ )
    PI_ASSERT( , c->consumer==thisproc.rank, PI_ENDPOINT_READER )

    int i;
    int arrayLen = -1;		// count received for ^ flag, or -1 if n/a
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];
    MPI_Status status;

    PI_BUNDLE *b = c->bundle;	// collective bundle associated with channel
    if ( b ) {			// NULL if point-to-point

        /* make sure we're on the rim of the bundled channel */
        PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_SYSTEM_ERROR )
        PI_ASSERT( , b->narrow_end==FROM, PI_BUNDLED_CHANNEL )
    }

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_LOCS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_READ][0], 0, NULL );    // mark start of PI_Read
#endif

    LOGCALL( "Rea", c->chan_id, format, 1, mpiArgCount, mpiArgs )

    /* Calculate format signature; if channel read, received writer's format and
     * compare; if bundle "read" (from Broadcast or Reduce), send format from our
     * "narrow" end of bundle for matchup.
     */
    if ( PI_CheckLevel >= 2 ) {
        int buff,
            sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        if ( b==NULL ) {
            PI_CALLMPI( MPI_Recv( &buff, 1, MPI_INT, c->producer,
                                  c->chan_tag, PI_CommWorld, &status ) )
            PI_ASSERT( LEVEL(2), buff==sig, PI_FORMAT_MISMATCH )
        }
        else {
            PI_CALLMPI( MPI_Bcast( &sig, 1, MPI_INT,	// what we're sending
                                   0, b->comm ) )	// "root" is rank 0 in bundle
        }
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* Reduce operation is never valid for PI_Read */
        PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

        /* Log each item */
        if ( i>0 ) LOGCALL( "Rea", c->chan_id, format, i+1, mpiArgCount, arg );

        /* First case handles ordinary receive and broadcast receive.
               MPI_Bcast here receives data from producer process within comm
               communicator (dedicated to this bundle).  In PI_Broadcast, the
               same MPI_Bcast sends the data. */

        if ( b==NULL || b->usage==PI_BROADCAST ) {

            /* Handling ^ flag or %s string step 1: expecting to receive array length first */
            if ( arg->sendCount ) {
                if ( b==NULL ) {
                    PI_CALLMPI( MPI_Recv( arg->buf, arg->count, arg->type, c->producer,
                                          c->chan_tag, PI_CommWorld, &status ) )
                }
                else {
                    PI_CALLMPI( MPI_Bcast(
                                    arg->buf, arg->count, arg->type,	// what we're getting
                                    0, b->comm ) )		// "root" is rank 0 in bundle
                }
                arrayLen = *(int *)arg->buf;
            }

            /* Step 2: malloc based on received array len, then get data  */
            else if ( arrayLen > 0 ) {
                /* Amount of storage = arrayLen * size of MPI type */
                int size;
                PI_CALLMPI( MPI_Type_size( arg->type, &size ) )

                /* Put array addr into user's pointer variable */
                *(void **)arg->buf = (void *)malloc( arrayLen * size );
                PI_ASSERT( , arg->buf != NULL, PI_MALLOC_ERROR );

                /* Now we're ready to receive the data */
                if ( b==NULL ) {
                    PI_CALLMPI( MPI_Recv( *(void **)arg->buf, arrayLen, arg->type, c->producer,
                                          c->chan_tag, PI_CommWorld, &status ) )

                }
                else {
                    PI_CALLMPI( MPI_Bcast(
                                    *(void **)arg->buf, arrayLen, arg->type,	// array we're getting
                                    0, b->comm ) )		// "root" is rank 0 in bundle
                }
                arrayLen = -1;		// done with arrayLen for this arg
            }

            /* Plain case: just receive the data */
            else {
                if ( b==NULL ) {
                    PI_CALLMPI( MPI_Recv( arg->buf, arg->count, arg->type, c->producer,
                                          c->chan_tag, PI_CommWorld, &status ) )
                }
                else {
                    PI_CALLMPI( MPI_Bcast(
                                    arg->buf, arg->count, arg->type,	// what we're sending
                                    0, b->comm ) )		// "root" is rank 0 in bundle
                }
            }
        }
        else {

            /* MPI_Scatterv here receives data from consumer process within comm
               communicator (dedicated to this bundle).  In PI_Scatter, the
               same MPI_Scatterv receives the data. */
            PI_CALLMPI( MPI_Scatterv(
                            NULL, NULL, NULL, 0,	// ignored on receiver call
                            arg->buf, arg->count, arg->type, // what we're receiving
                            0, b->comm ) )		// "root" is rank 0 in bundle
        }
    }

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        MPE_Log_receive( c->producer, c->chan_tag, arrayLen );  // receiver's end of message arrow

        bytebuf_pos = 0;
        int namelen = strlen( c->name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_CHANNEL_SMAX,namelen), c->name );
        MPE_Log_event( thisproc.mpe_event[LOG_CHANNEL], 0, bytebuf );   // event bubble in PI_Read

        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        namelen = strlen( thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_READ_SMAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        MPE_Log_event( thisproc.mpe_eventse[LOG_READ][1], 0, bytebuf ); // mark end of PI_Read
    }
#endif
}

int PI_Select_( PI_BUNDLE *b )
{
    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_SELECT, PI_BUNDLE_USAGE )
    PI_ASSERT( , b->narrow_end==TO, PI_ENDPOINT_READER )

    MPI_Status status;
    int i;

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_SELECT][0], 0, NULL ); // mark start of PI_Select
#endif

    LOGCALL( "Sel", b->bund_id, "", 0, 0, NULL )

    PI_CALLMPI( MPI_Probe( MPI_ANY_SOURCE, b->channels[0]->chan_tag,
                           PI_CommWorld, &status ) )

    /* note: this is a sequential search! suppose bundle is large?
       May want to build (on the fly) lookup table (hash?) for rank=>index.
       Another alt (since each rank will likely participate in few bundles) is
       to make small lookup table in rank's PI_PROCENVT: [rank].table:tag=>index.
       The table could be PI_MAX_BUNDLES long, and CreateBundle fills in the next entry
       for each producer process.
    */

    for ( i = 0; i < b->size; i++ ) {
        if ( b->channels[i]->producer == status.MPI_SOURCE ) {
#ifdef PILOT_WITH_MPE
            if ( thisproc.svc_flag[LOG_MPE] ) {
                bytebuf_pos = 0;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
                int namelen = strlen(thisproc.processes[thisproc.rank].name);
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_SELECT_S1MAX,namelen), thisproc.processes[thisproc.rank].name );
                MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
                namelen = strlen(b->name);
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_SELECT_S2MAX,namelen), b->name );
                MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &i );
                MPE_Log_event( thisproc.mpe_eventse[LOG_SELECT][1], 0, bytebuf );   // mark end of PI_Select
            }
#endif
            return i;
        }
    }

    /* If the message source does not match the producer of any of the bundle's
       channels, that's a problem.  PI_ASSERT(, 0, ...) will always abort. */
    PI_ASSERT( , 0, PI_SYSTEM_ERROR )
    return 0;	// never reached, but satisfies compiler
}

int PI_ChannelHasData_( PI_CHANNEL *c )
{
    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , c, PI_NULL_CHANNEL )
    PI_ASSERT( LEVEL(1), ISVALID(PI_CHAN,c), PI_INVALID_OBJ )

    int flag;
    MPI_Status s;

    LOGCALL( "Has", c->chan_id, "", 0, 0, NULL )

    PI_CALLMPI( MPI_Iprobe( c->producer, c->chan_tag, PI_CommWorld, &flag, &s ) )

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(c->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_CHANNELHASDATA,namelen), c->name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &flag );
        MPE_Log_event( thisproc.mpe_event[LOG_CHANNELHASDATA], 0, bytebuf );    // event bubble for PI_ChannelHasData result
    }
#endif

    return flag;
}

int PI_TrySelect_( PI_BUNDLE *b )
{
    PI_ON_ERROR_RETURN( -1 )
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_SELECT, PI_BUNDLE_USAGE )
    PI_ASSERT( , b->narrow_end==TO, PI_ENDPOINT_READER )

    int flag, i;
    MPI_Status status;

    LOGCALL( "Try", b->bund_id, "", 0, 0, NULL )

    PI_CALLMPI( MPI_Iprobe( MPI_ANY_SOURCE, b->channels[0]->chan_tag,
                            PI_CommWorld, &flag, &status ) )

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(b->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_TRYSELECT_SMAX,namelen), b->name );
    }
#endif

    if ( flag == 0 ) {
#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {
            int return_val = -1;
            MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &return_val );
            MPE_Log_event( thisproc.mpe_event[LOG_TRYSELECT], 0, bytebuf );     // event bubble for PI_TrySelect result
        }
#endif

        return -1;		// no channel has data
    }

    /* lookup message source's corresponding channel index in bundle
       (see comment in PI_Select_ re sequential search) */
    for ( i = 0; i < b->size; i++ ) {
        if ( b->channels[i]->producer == status.MPI_SOURCE ) {
#ifdef PILOT_WITH_MPE
            if ( thisproc.svc_flag[LOG_MPE] ) {
                MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &i );
                MPE_Log_event( thisproc.mpe_event[LOG_TRYSELECT], 0, bytebuf ); // event bubble for PI_TrySelect result
            }
#endif
            return i;           // channel index with data
        }
    }

    /* If the message source does not match the producer of any of the bundle's
       channels, that's a problem.  PI_ASSERT(, 0, ...) will always abort. */
    PI_ASSERT( , 0, PI_SYSTEM_ERROR )
    return 0;	// never reached, but satisfies compiler
}

PI_CHANNEL *PI_GetBundleChannel_( const PI_BUNDLE *b, int index )
{
    PI_ON_ERROR_RETURN( NULL )
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , index >= 0 && index < b->size, PI_BUNDLE_INDEX )

    return b->channels[index];
}

int PI_GetBundleSize_( const PI_BUNDLE *b )
{
    PI_ON_ERROR_RETURN( 0 )
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )

    return b->size;
}


void PI_Broadcast_( PI_BUNDLE *b, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_BROADCAST, PI_BUNDLE_USAGE )
    PI_ASSERT( , thisproc.rank==b->channels[0]->producer, PI_ENDPOINT_WRITER )

    int i;
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_VALS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_BROADCAST][0], 0, NULL );       // mark start of PI_Broadcast
#endif

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
    LOGCALL( "Bro", b->bund_id, format, 1, mpiArgCount, mpiArgs )

    /* Calculate format signature; send from our "narrow" end of bundle to readers
     * for matchup.
     */
    if ( PI_CheckLevel >= 2 ) {
        int sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        PI_CALLMPI( MPI_Bcast( &sig, 1, MPI_INT,	// what we're sending
                               0, b->comm ) )	// "root" is rank 0 in bundle
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* Reduce operation is never valid for PI_Broadcast */
        PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

        /* Log each item */
        if ( i>0 ) LOGCALL( "Bro", b->bund_id, format, i+1, mpiArgCount, arg );

#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {
            int j;
            for ( j = b->size-1; j >= 0; j-- ) {          // fan out message arrows to PI_Readers
                mybuff[0] = '\0';
                bytebuf_pos = 0;
                // skip leading PI_LOGSEP char output by interpArg
                int namelen = strlen( interpArg(mybuff, LOG_BROADCAST_MSG_SMAX, "Bro", &mpiArgs[i]) ) - 1;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_BROADCAST_MSG_SMAX,namelen), mybuff+1 );
                MPE_Log_event( thisproc.mpe_event[LOG_BROADCAST_MSG], 0, bytebuf );     // event bubble in PI_Broadcast
                MPE_Log_send( b->channels[j]->consumer, b->channels[j]->chan_tag, arg->count ); // sender's end of message arrow
                usleep(1000);   // pause 1 msec. so message arrows don't get superimposed in logfile
            }
        }
#endif

        PI_CALLMPI( MPI_Bcast(
                        arg->buf, arg->count, arg->type,	// what we're sending
                        0, b->comm ) )		// "root" is rank 0 in bundle
    }
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(thisproc.processes[thisproc.rank].name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_BROADCAST_S1MAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        namelen = strlen(b->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_BROADCAST_S2MAX,namelen), b->name );
        MPE_Log_event( thisproc.mpe_eventse[LOG_BROADCAST][1], 0, bytebuf );    // mark end of PI_Broadcast
    }
#endif
}


void PI_Scatter_( PI_BUNDLE *b, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_SCATTER, PI_BUNDLE_USAGE )
    PI_ASSERT( , thisproc.rank==b->channels[0]->producer, PI_ENDPOINT_WRITER )

    int i, chan;
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_LOCS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_SCATTER][0], 0, NULL ); // mark start of PI_Scatter
#endif

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
    LOGCALL( "Sca", b->bund_id, format, 1, mpiArgCount, mpiArgs )

    /* Calculate format signature; send from our "narrow" end of bundle to readers
     * for matchup.
     */
    if ( PI_CheckLevel >= 2 ) {
        int sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        PI_CALLMPI( MPI_Bcast( &sig, 1, MPI_INT,	// what we're sending
                               0, b->comm ) )	// "root" is rank 0 in bundle
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* Reduce operation is never valid for PI_Scatter */
        PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

        /* set up args for MPI_Scatter (sending side) */
        int sendcounts[b->size+1];	// count sent to each process
        int displs[b->size+1];		// displacements in userbuf for send

        /* Log each item */
        if ( i>0 ) LOGCALL( "Sca", b->bund_id, format, i+1, mpiArgCount, arg );

#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {                     // fan out message arrows to PI_Readers
            int j;
            for ( j = b->size - 1; j >= 0; j-- ) {
                mybuff[0] = '\0';
                bytebuf_pos = 0;
                // skip leading PI_LOGSEP char output by interpArg
                int namelen = strlen( interpArg(mybuff, LOG_SCATTER_MSG_SMAX, "Sca", &mpiArgs[i]) ) - 1;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_SCATTER_MSG_SMAX,namelen), mybuff+1 );
                MPE_Log_event( thisproc.mpe_event[LOG_SCATTER_MSG], 0, bytebuf );       // event bubble in PI_Scatter
                MPE_Log_send( b->channels[j]->consumer, b->channels[j]->chan_tag, arg->count ); // sender's end of message arrow
                usleep(1000);   // pause 1 msec. so message arrows don't get superimposed in logfile
            }
        }
#endif

        /* prepare sendcounts and displs arrays so that root receives nothing,
           and all the rest receive 'count' items */
        sendcounts[0] = displs[0] = 0;
        for ( chan=1; chan<=b->size; chan++ ) {
            sendcounts[chan] = arg->count;
            displs[chan] = (chan-1) * arg->count;	// fits buffer of size*count items
        }

#ifdef MPI_IN_PLACE
        PI_CALLMPI( MPI_Scatterv(
                        arg->buf, sendcounts, displs, arg->type,	// sends all data
                        MPI_IN_PLACE, 0, 0,	// receive 0 data from "root"
                        0, b->comm ) )		// "root" is P0 in bundle communicator
#else
        char recvbuf[1];	// root receives 0-length data, so make dummy
        PI_CALLMPI( MPI_Scatterv(
                        arg->buf, sendcounts, displs, arg->type,	// sends all data
                        recvbuf, 0, arg->type,	// receive 0 data from "root"
                        0, b->comm ) )		// "root" is P0 in bundle communicator
#endif
    }
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(thisproc.processes[thisproc.rank].name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_SCATTER_S1MAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        namelen = strlen(b->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_SCATTER_S2MAX,namelen), b->name );
        MPE_Log_event( thisproc.mpe_eventse[LOG_SCATTER][1], 0, bytebuf );      // mark end of PI_Scatter
    }
#endif
}


void PI_Reduce_( PI_BUNDLE *b, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_REDUCE, PI_BUNDLE_USAGE )
    PI_ASSERT( , thisproc.rank==b->channels[0]->consumer, PI_ENDPOINT_READER )

    int i;
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];
    MPI_Status status;

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_LOCS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_REDUCE][0], 0, NULL );  // mark start of PI_Reduce
#endif

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
    LOGCALL( "Rdu", b->bund_id, format, 1, mpiArgCount, mpiArgs )

    /* The reduction topology is complex: The writers are in a communicator, and
     * the "rank 0" member (=the root) is nominated to get the reduced result and
     * send it to the PI_Reduce process. So as the latter, we obey the "reader
     * validates the format" rule: we receive the writer's format and compare.
     */
    if ( PI_CheckLevel >= 2 ) {
        int buff,
            sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        PI_CALLMPI( MPI_Recv( &buff, 1, MPI_INT,
                              b->channels[0]->producer, b->channels[0]->chan_tag,
                              PI_CommWorld, &status ) )
        PI_ASSERT( LEVEL(2), buff==sig, PI_FORMAT_MISMATCH )
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* PI_Reduce doesn't actually use the operator, but we insist on it
           for consistency. */
        PI_ASSERT( , arg->op!=MPI_OP_NULL, PI_OP_MISSING )

        /* Log each item */
        if ( i>0 ) LOGCALL( "Rdu", b->bund_id, format, i+1, mpiArgCount, arg );

        /* The actual reduction is done within the communicator, i.e., by the
           processes on the bundle's rim.  The 1st channel's producer process
           will send the result back here. */
        PI_CALLMPI( MPI_Recv( arg->buf, arg->count, arg->type,
                              b->channels[0]->producer, b->channels[0]->chan_tag,
                              PI_CommWorld, &status ) )
#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {                     // fan in message arrows from PI_Writers
            int j;
            for ( j = 0; j < b->size; j++ ) {
                MPE_Log_receive( b->channels[j]->producer, b->channels[j]->chan_tag, arg->count );  // receiver's end of message arrow
                bytebuf_pos = 0;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', strlen( b->channels[j]->name ), b->channels[j]->name );
                MPE_Log_event( thisproc.mpe_event[LOG_CHANNEL], 0, bytebuf );   // event bubble in PI_Reduce
                usleep(1000);   // pause 1 msec. so message arrows don't get superimposed in logfile
            }
        }
#endif
    }
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(thisproc.processes[thisproc.rank].name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_REDUCE_S1MAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        namelen = strlen(b->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_REDUCE_S2MAX,namelen), b->name );
        MPE_Log_event( thisproc.mpe_eventse[LOG_REDUCE][1], 0, bytebuf );       // mark end of PI_Reduce
    }
#endif
}


void PI_Gather_( PI_BUNDLE *b, const char *format, ... )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )
    PI_ASSERT( , b, PI_NULL_BUNDLE )
    PI_ASSERT( , format, PI_NULL_FORMAT )
    PI_ASSERT( LEVEL(1), ISVALID(PI_BUND,b), PI_INVALID_OBJ )
    PI_ASSERT( , b->usage==PI_GATHER, PI_BUNDLE_USAGE )
    PI_ASSERT( , thisproc.rank==b->channels[0]->consumer, PI_ENDPOINT_READER )

    int i, chan;
    va_list argptr;
    int mpiArgCount;
    PI_MPI_RTTI mpiArgs[ PI_MAX_FORMATLEN ];

    va_start( argptr, format );
    mpiArgCount = ParseFormatString( IO_CONTEXT_LOCS, mpiArgs, format, argptr );
    va_end( argptr );
    if ( mpiArgCount < 0 ) return;	// func. detected error with PI_OnErrorReturn
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] )
        MPE_Log_event( thisproc.mpe_eventse[LOG_GATHER][0], 0, NULL );  // mark start of PI_Gather
#endif

    /* Log the first item, so that if the format message causes a deadlock (which it
     * likely will if the subsequent I/O would cause one), it will get diagnosed.
     */
    LOGCALL( "Gat", b->bund_id, format, 1, mpiArgCount, mpiArgs )

    /* Calculate format signature; send from our "narrow" end of bundle to writers
     * for matchup.
     */
    if ( PI_CheckLevel >= 2 ) {
        int sig = (int)FormatSignature( mpiArgs, mpiArgCount );

        PI_CALLMPI( MPI_Bcast( &sig, 1, MPI_INT,	// what we're sending
                               0, b->comm ) )	// "root" is rank 0 in bundle
    }

    for ( i = 0; i < mpiArgCount; i++ ) {
        PI_MPI_RTTI* arg = &mpiArgs[ i ];

        /* Reduce operation is never valid for PI_Gather */
        PI_ASSERT( , arg->op==MPI_OP_NULL, PI_OP_INVALID )

        /* set up args for MPI_Gather (receiving side) */
        int recvcounts[b->size+1];	// count that each process sends
        int displs[b->size+1];		// displacements in userbuf for recv

        /* Log each item */
        if ( i>0 ) LOGCALL( "Gat", b->bund_id, format, i+1, mpiArgCount, arg );

        /* prepare recvcounts and displs arrays so that root sends nothing,
           and all the rest send 'count' items */
        recvcounts[0] = displs[0] = 0;
        for ( chan=1; chan<=b->size; chan++ ) {
            recvcounts[chan] = arg->count;
            displs[chan] = (chan-1) * arg->count;	// fits buffer of size*count items
        }

#ifdef MPI_IN_PLACE
        PI_CALLMPI( MPI_Gatherv(
                        MPI_IN_PLACE, 0, 0,	// send no data from "root"
                        arg->buf, recvcounts, displs, arg->type,	// receives all data
                        0, b->comm ) )		// "root" is P0 in bundle communicator
#else
        char sendbuf[1];	// root sends 0-length data, so make dummy
        PI_CALLMPI( MPI_Gatherv(
                        sendbuf, 0, arg->type,	// send 0 data from "root"
                        arg->buf, recvcounts, displs, arg->type,	// receives all data
                        0, b->comm ) )		// "root" is P0 in bundle communicator
#endif

#ifdef PILOT_WITH_MPE
        if ( thisproc.svc_flag[LOG_MPE] ) {                     // fan in message arrows from PI_Writers
            int j;
            for ( j = 0; j < b->size; j++ ) {
                MPE_Log_receive( b->channels[j]->producer, b->channels[j]->chan_tag, arg->count );
                bytebuf_pos = 0;
                MPE_Log_pack( bytebuf, &bytebuf_pos, 's', strlen( b->channels[j]->name ), b->channels[j]->name );
                MPE_Log_event( thisproc.mpe_event[LOG_CHANNEL], 0, bytebuf );   // event bubble in PI_Gather
                usleep(1000);   // pause 1 msec. so message arrows don't get superimposed in logfile
            }
        }
#endif
    }
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int namelen = strlen(thisproc.processes[thisproc.rank].name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_GATHER_S1MAX,namelen), thisproc.processes[thisproc.rank].name );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &thisproc.processes[thisproc.rank].argument );
        namelen = strlen(b->name);
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_GATHER_S2MAX,namelen), b->name );
        MPE_Log_event( thisproc.mpe_eventse[LOG_GATHER][1], 0, bytebuf );       // mark end of PI_Gather
    }
#endif
}

void PI_Log_( const char *text )
{
    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )

    /* if logging to file enabled, forward to LogEvent as USER type event */
    if ( thisproc.svc_flag[OLP_LOGFILE] ) LogEvent( USER, text );

#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_pack( bytebuf, &bytebuf_pos, 'd', 1, &PI_CallerLine );
        int textlen = strlen( text );
        MPE_Log_pack( bytebuf, &bytebuf_pos, 's', MIN(LOG_LOG_SMAX,textlen), text );
        MPE_Log_event( thisproc.mpe_event[LOG_LOG], 0, bytebuf );       // event bubble for PI_Log
    }
#endif
}

/* This function might be called before Pilot's tables are all set up, so be
   sure we don't deref. any NULL pointers.  It can be called in any phase of
   the program by any process.  If MPI is not even running yet, it will call
   abort() and get out that way.  If Pilot is in the configuration phase (where
   the same code is running on all nodes), only rank 0 will print, to avoid
   cluttering the display with many duplicate messages.
*/
void PI_Abort( const int errcode, const char *text, const char *file, const int line )
{
    if ( thisproc.phase != CONFIG || thisproc.rank == 0 ) {
        char *procname = "";
        int procarg = 0;
        if ( thisproc.processes ) {
            procname = thisproc.processes[thisproc.rank].name;
            procarg = thisproc.processes[thisproc.rank].argument;
        }
        fprintf( stderr, "\n" PI_BORDER
                 "*** PI_Abort *** Called from MPI process #%d = Pilot process:\n"
                 PI_BORDER "***   %s(%d) @ %s:%d:\n"
                 PI_BORDER "***   %s%s\n" PI_BORDER
                 "The above should pinpoint the problem.\n" PI_BORDER
                 "Subsequent messages from MPI can likely be disregarded.\n",
                 thisproc.rank, procname, procarg, file, line,
                 ( errcode >= PI_MIN_ERROR && errcode <= PI_MAX_ERROR ) ?
                 ErrorText[errcode] : "",
                 text ? text : "" );
    }

    /* TODO: Shut down MPE logging gracefully so file isn't lost or
       corrupted (MPI errors are intercepted and end up here, too).
       Unfortunately, we only have control over one process -- this one --
       indeed, others could be exited or deadlocked, so we can't arrange for
       them to call MPE functions.  We know that MPI_Abort, at least with
       OpenMPI, sends SIGTERM to all processes, and we *can* trap that, but
       MPE calls in the handler don't seem to work or even return, probably
       because they in turn use MPI and MPI is shutting down.  So we have
       to give up on this :-(
       Another approach would be to find a Pilot way to stop all the processes
       without calling MPI_Abort (at least not till everything is tidied up). */

    /* MPI should shut down the application and may call exit/abort(errcode) */
    int MPI_up;
    MPI_Initialized( &MPI_up );
    if ( MPI_up )
        MPI_Abort( MPI_COMM_WORLD, errcode );	// kill everyone, not just PI_CommWorld subset
    else
        abort();
    /******** does not return *********/
}

void PI_StopMain_( int status )
{
    int i;

    PI_ON_ERROR_RETURN()
    PI_ASSERT( , thisproc.phase==RUNNING, PI_WRONG_PHASE )

    /* First job is to shutdown Pilot process logging, if it's active. */
    if ( thisproc.svc_flag[OLP_RANK] == 1 ) {

        /* Notify log that this process is finished (unless this *is* the log
           process on rank 1) */
        if ( thisproc.rank != 1 ) {
            char buff[PI_MAX_LOGLEN];
            sprintf( buff, "FIN" PI_LOGSEP "%d", status );
            LogEvent( PILOT, buff );
        }
    }

    /*MPE Shutdown Calls*/
#ifdef PILOT_WITH_MPE
    if ( thisproc.svc_flag[LOG_MPE] ) {
        bytebuf_pos = 0;
        MPE_Log_event( thisproc.mpe_eventse[LOG_COMPUTE][1], 0, NULL ); // end of execution phase (Compute state)
        MPE_Log_sync_clocks();
        MPE_Finish_log( LogFilename );
    }
#endif

    MPI_Barrier( PI_CommWorld );	/* synchronize all processes */

    /* If user pre-initialized MPI, then leave it initialized.  This is to
       allow Pilot to be re-configured and used again in this program, which is
       needed for running benchmarks, but not in ordinary use.  In this case,
       since MPI_Finalize is not being called, we take the trouble to free up
       the communicators created for non-selector bundles, because MPI is liable
       to run out of that resource given enough repetitions.
    */
    if ( MPIPreInit ) {
        thisproc.phase = PREINIT;
        if ( thisproc.bundles != NULL )
            for ( i = 0; i < thisproc.allocated_bundles; i++ ) {
                if ( thisproc.bundles[i]->usage != PI_SELECT &&
                        thisproc.bundles[i]->comm != MPI_COMM_NULL )
                    MPI_Comm_free( &(thisproc.bundles[i]->comm) );
            }
    }
    else {
        thisproc.phase = POSTRUN;
        MPI_Finalize( );
    }

    /* deallocate memory */

    for ( i = 0; i < thisproc.allocated_channels; i++ )
        free( thisproc.channels[i] );

    if ( thisproc.channels != NULL )
        free( thisproc.channels );

    if ( thisproc.processes != NULL )
        free( thisproc.processes );

    if ( thisproc.bundles != NULL ) {
        for( i = 0; i < thisproc.allocated_bundles; i++ ) {
            if ( thisproc.bundles[i]->channels != NULL )
                free( thisproc.bundles[i]->channels );
        }
        free( thisproc.bundles );
    }

    if ( LogFilename ) free( LogFilename );

    /* The main process always returns, but other processes normally exit
       here because otherwise they would return from PI_StartAll and (re)execute
       main's code.  However, if Pilot is in "bench mode", we do return so
       that another Pilot application can be configured and executed.
    */
    if ( thisproc.rank!=0 && !MPIPreInit ) exit( 0 );
}


/*** Internal Functions ***/

/*!
********************************************************************************
Called by MPI to handle error in library function.
Handle the error reported from MPI by printing message and aborting.
Could be due to Pilot's misuse of MPI, or MPI detecting a user error (like
mismatched read/write data types or lengths).

\param comm MPI communicator where error occurred (we don't use this).
\param code MPI error code
\param ... additional implementation-specific args (ignore)
*******************************************************************************/
static void HandleMPIErrors( MPI_Comm *comm, int *code, ... )
{
    char buff[MPI_MAX_ERROR_STRING], text[MPI_MAX_ERROR_STRING+80];
    int len;

    MPI_Error_string( *code, buff, &len );	// obtain text for error code

    snprintf( text, sizeof(text), " at " __FILE__ ":%d; MPI error code %d:\n%s",
              MPICallLine, *code, buff );

    PI_Abort( PI_MPI_ERROR, text, PI_CallerFile, PI_CallerLine );
    /***** does not return *****/
}

/*!
********************************************************************************
Parse command-line arguments to Pilot. Fills in #Option.
*******************************************************************************/
static char *ParseArgs( int *argc, char ***argv )
{
    int i, j, unrec, count=0, moveto=0;
    char *badargs = NULL;		// assume all -pi... args will be OK

    memset( Option, 0, OPT_END );	// clear all option flags
    LogFilename = NULL;			// assume no log needed
    OnlineProcess = OLP_NONE;		// assume no online process needed

    /* scan args, shuffling non-Pilot args up in *argv array */
    for ( i=0; i<*argc; i++ ) {
        unrec = 0;
        if ( 0==strncmp( (*argv)[i], "-pi", 3 ) ) {	// -pi... arg found

            /* '-pisvc=...' runtime services */
            if ( 0==strncmp( (*argv)[i]+3, "svc=", 4 ) ) {
                for ( j=7; (*argv)[i][j]; j++ ) {
                    switch ( toupper( (*argv)[i][j] ) ) {
                    case 'C':
                        Option[OPT_CALLS] = 1;
                        break;
                    case 'D':
                        Option[OPT_DEADLOCK] = 1;
                        break;
                    case 'J':
                        Option[OPT_JUMPSHOT] = 1;
                        break;
                    case 'S':
                        Option[OPT_STATS] = 1;  // future
                        break;
                    case 'M':
                        Option[OPT_TOPO] = 1;   // future
                        break;
                    case 'T':
                        Option[OPT_TRACE] = 1;  // future
                        break;
                    default:
                        unrec = 1;
                    }
                }
            }

            /* '-pilog=filename' */
            else if ( 0==strncmp( (*argv)[i]+3, "log=", 4 ) ) {
                int len = strlen( (*argv)[i] );
                if ( len < 8 ) unrec = 1;
                else LogFilename = strdup( (*argv)[i]+7 );	// found filename
            }

            /* '-picheck=n' */
            else if ( 0==strncmp( (*argv)[i]+3, "check=", 6 ) ) {
                if ( 10==strlen( (*argv)[i] ) ) {
                    j = (*argv)[i][9];	// extract level number
                    if ( isdigit(j) ) PI_CheckLevel = j - '0';
                    else unrec = 1;
                }
                else unrec = 1;
            }

            else unrec = 1;

            /* save up all unrecognized arguments to return to caller */
            if ( unrec ) {
                j = badargs ? strlen(badargs) : 0;	// current string len
                badargs = realloc( badargs, j + strlen((*argv)[i]) + 1 );
                strcpy( badargs+j, " " );
                strcpy( badargs+j+1, (*argv)[i] );
            }
        }
        else {
            if ( count < i ) (*argv)[moveto] = (*argv)[i];
            count++;
            moveto++;
        }
    }
    *argc = count;	// adjust remaining arg count

    return badargs;
}


/*!
********************************************************************************
This thread is used to invoke OnlineProcessFunc when the latter has to run
as a thread on node 0.
*******************************************************************************/
/*static void *OnlineThreadFunc( void *arg )
{
    OnlineProcessFunc( 0, NULL );
    return NULL;    // thread will be joined by PI_StopMain
}*/


/*!
********************************************************************************
This process handles collecting/printing log entries, and running any other
online services such as deadlock detection.  The log filename, if any,
is received from PI_MAIN.

Currently, the log timestamp is being added here. MPI semantics guarantee
that the events from any given process will be logged in order, but there
is no guarantee of a total ordering of all events from all processes. It
would be more accurate to have PI_Log timestamp the event on its own node,
then have this function apply a correction, like how MPE tries to synchronize
all the clocks.

\note If the program crashes, log entries -- maybe the entire file -- may be
lost in OS buffers. We should probably do some selective periodic flushing,
but not every line to avoid burdening the program with disk activity. OTOH,
the burden is not serious if the log file is being written on a dedicated node,
especially on its local filesystem (e.g., /tmp). For now, we are flushing after
every write.
*******************************************************************************/
static int OnlineProcessFunc( int dum1, void *dum2 )
{
    MPI_Status stat;
    int flen;
    char *fname;
    char *extension = ".log";
    FILE *logfile = NULL;
    char buff[PI_MAX_LOGLEN];

    double start = MPI_Wtime();     // capture time at start of run

    /* get filename length; 0 = no log file */
    PI_CALLMPI( MPI_Recv( &flen, 1, MPI_INT, PI_MAIN, 0, PI_CommWorld, &stat ) )

    /* open the log file if needed */
    if ( flen ) {
        PI_OLP_ASSERT( fname = malloc( flen+strlen(extension) ), PI_MALLOC_ERROR )
        PI_CALLMPI( MPI_Recv( fname, flen, MPI_CHAR,
                              PI_MAIN, 0, PI_CommWorld, &stat ) )
        strcat( fname, extension );
        logfile = fopen( fname, "w" );
        if ( NULL==logfile )
            PI_Abort( PI_LOG_OPEN, fname, __FILE__, __LINE__  );
        /***** does not return *****/

        free( fname );
    }

    if ( thisproc.svc_flag[LOG_TABLES] );   // dump tables to log file
    // might be easier to do in PI_MAIN with calls to LogEvent

    /* startup other OLPs */
    if ( thisproc.svc_flag[OLP_DEADLOCK] ) PI_DetectDL_start_( &thisproc );


    /******** main loop till "FIN" messages ********/

    /* no. of FINs that have to check in (1 less if we are Pilot process) */
    int FINs = thisproc.worldsize - thisproc.svc_flag[OLP_RANK];
    while ( FINs > 0 ) {
        /* wait for next message from LogEvent */
        PI_CALLMPI( MPI_Recv( buff, PI_MAX_LOGLEN, MPI_CHAR,
                              MPI_ANY_SOURCE, 0, PI_CommWorld, &stat ) )

        char *event = buff;
        int contchar = PI_MAX_LOGLEN - 1;   // index of continuation char
        int source = stat.MPI_SOURCE;

        /* receive rest of message while continuation char found */
        while ( event[contchar] == '+' ) {
            if ( event == buff )            // first extension
                event = strdup( buff );     // get msg in dynamic storage
            int msglen = strlen( event );
            PI_OLP_ASSERT( event = realloc( event, msglen+PI_MAX_LOGLEN ),
                           PI_MALLOC_ERROR )   // extend buffer

            /* continue receiving just from source of partial message */
            PI_CALLMPI( MPI_Recv( event+msglen, PI_MAX_LOGLEN, MPI_CHAR,
                                  source, 0, PI_CommWorld, &stat ) )
            contchar = msglen + PI_MAX_LOGLEN - 1;
        }

        /* forward to OLP if event type is one it wants */
        if ( thisproc.svc_flag[OLP_DEADLOCK] )
            if ( event[0] == PILOT || event[0] == CALLS )
                PI_DetectDL_event_( event );

        /* get timestamp (nsec from start) and write event to log file */
        if ( logfile ) {
            fprintf( logfile, "%.6ld" PI_LOGSEP "%s\n",
                     (long int)((MPI_Wtime()-start)*1000000), event );
            fflush( logfile );		// in case program crashes or PI_Aborts
        }

        /* check for "P_n_FIN" pattern, where P is the PILOT message type char,
            _ is the separator, and n is the process number.  We need to get
           one from all processes, so decrement counter.
        */
        if ( event[0] == PILOT ) {
            char *sep = strpbrk( event+2, PI_LOGSEP );   // skip over P_n
            if ( sep && 0==strncmp( sep+1, "FIN", 3 ) ) FINs--;
        }

        if ( event != buff ) free( event ); // it was malloc'd
    }

    /* terminate OLPs */
    if ( thisproc.svc_flag[OLP_DEADLOCK] ) PI_DetectDL_end_();

    if ( thisproc.svc_flag[LOG_STATS] );    // TODO dump stats to log file
    // who collects the stats?

    if ( logfile ) fclose( logfile );

    return 0;
}


/*!
********************************************************************************
Interpret a given read/write format argument for the purpose of logging it.

Don't let dest grow beyond maxlen (including NUL).
If arg is NULL, leave dest unchanged.

This is primitive right now, only printing the data length (MPI count) and
16 hex digits from the arg.data union. We could look at the datatype (arg->cType)
and the direction (need addtional arg to lookup "Rea" "Wri" "Bro" etc.).

Currently this prints the arg's value if it's an output type code, or arg's
address if it's an input type code.  For output, might be nice to print the
last value, too.

It always starts by inserting a PI_LOGSEP separator character.
*******************************************************************************/
static const char *interpArg( char *dest, int maxlen, const char *code, const PI_MPI_RTTI *arg )
{
    if ( arg == NULL ) return dest;	// no arg case

    // use code to decide whether this is an input or output call

    const char *iocodes = "Rea" "Gat" "Rdu" "Wri" "Sca" "Bro";
    char *which = strstr( iocodes, code );
    if ( which==NULL ) return dest;	// nothing to print

    int func = (which - iocodes) / 3;	// convert to function number

    if ( func < 3 ) {		// input type function, print address
        snprintf( dest+strlen(dest), maxlen, PI_LOGSEP "[%d] %p", arg->count, arg->data.address);
        return dest;
    }

    // output case: location of data depends on length

    if ( arg->count == 1 )	// single value is stored in arg
    {

        // print the value from arg's union according to its type

#define PRVAL(FORMAT,UNION) \
  snprintf( dest+strlen(dest), maxlen, PI_LOGSEP "[%d] " FORMAT, \
	    arg->count, arg->data.UNION ); \
  break;

        // default output for unsupported types, just count and '?'

#define PRFAIL() \
  snprintf( dest+strlen(dest), maxlen, PI_LOGSEP "[%d] ?", \
	    arg->count ); \
  break;

// *INDENT-OFF*
        switch ( arg->cType ) {
        case CTYPE_CHAR:                PRVAL("%c",c)
        case CTYPE_SHORT:               PRVAL("%hd",h)
        case CTYPE_INT:                 PRVAL("%d",d)
        case CTYPE_LONG:                PRVAL("%ld",ld)
        case CTYPE_UNSIGNED_CHAR:       PRVAL("%hhu",hhu)
        case CTYPE_UNSIGNED_SHORT:      PRVAL("%hu",hu)
        case CTYPE_UNSIGNED_LONG:       PRVAL("%lu",lu)
        case CTYPE_UNSIGNED:            PRVAL("%u",u)
        case CTYPE_FLOAT:               PRVAL("%f",f)
        case CTYPE_DOUBLE:              PRVAL("%lf",lf)
        case CTYPE_LONG_DOUBLE:         PRVAL("%Lf",llf)
        case CTYPE_BYTE:                PRVAL("%#2x",b)
        case CTYPE_LONG_LONG:           PRVAL("%lld",lld)
        case CTYPE_UNSIGNED_LONG_LONG:  PRVAL("%llu",llu)
        case CTYPE_USER_DEFINED:        PRVAL("%p",address)

        // don't try to handle these types yet
        case CTYPE_FORTRAN:             PRFAIL()
        default:
            break;
        }
        return dest;
    }

    // print the first value from arg's address according to its type

#define PRADDR(FORMAT,CAST) \
  snprintf( dest+strlen(dest), maxlen, PI_LOGSEP "[%d] " FORMAT "...", \
	    arg->count, *(CAST*)arg->data.address ); \
  break;

    switch ( arg->cType ) {
    case CTYPE_CHAR:            PRADDR("%c",char)
    case CTYPE_SHORT:           PRADDR("%hd",short)
    case CTYPE_INT:             PRADDR("%d",int)
    case CTYPE_LONG:            PRADDR("%ld",long)
    case CTYPE_UNSIGNED_CHAR:   PRADDR("%hhu",unsigned char)
    case CTYPE_UNSIGNED_SHORT:  PRADDR("%hu",unsigned short)
    case CTYPE_UNSIGNED_LONG:   PRADDR("%lu",unsigned long)
    case CTYPE_UNSIGNED:        PRADDR("%u",unsigned)
    case CTYPE_FLOAT:           PRADDR("%f",float)
    case CTYPE_DOUBLE:          PRADDR("%lf",double)
    case CTYPE_LONG_DOUBLE:     PRADDR("%Lf",long double)
    case CTYPE_BYTE:            PRADDR("%#2x",unsigned char)
    case CTYPE_LONG_LONG:       PRADDR("%lld",long long)
    case CTYPE_UNSIGNED_LONG_LONG: PRADDR("%llu",unsigned long long)

    // don't try to handle these types
    case CTYPE_FORTRAN:         PRFAIL()
    case CTYPE_USER_DEFINED:    PRFAIL()
    default:
        break;
    }
// *INDENT-ON*
    return dest;
}


/*!
********************************************************************************
Add process number and send to online process.  Format of fixed len message:

 		Pn_t_event...c

  - Pn is the process no. (rank)	<- can't sort this w/o leading 0's
  - _ is the field separator PI_LOGSEP
  - t is the numeric log event type
  - event is as much of the text as will fit, leaving room for \\0
  - c is the continuation character '+' or space for last line

Overflow will only be an issue with user events (via PI_Log).
Rank of OLP is in svc_flag[OLP_RANK]. We use tag=0 since it is not assigned
for channels.
\note This can probably be improved with non-blocking sends (for the last
line), but then the function won't be reentrant (which probably doesn't
matter).
*******************************************************************************/
static void LogEvent( LOGEVENT ev, const char *event )
{
    // clear buffer so valgrind doesn't find "uninitialized storage" in MPI_Send
    char buff[PI_MAX_LOGLEN] = {0};

    int len = snprintf( buff, PI_MAX_LOGLEN-1,
                        "%c" PI_LOGSEP "%d" PI_LOGSEP "%s",
                        (char)ev, thisproc.rank, event );

    /* len >= LOGLEN-1 means that overflow bytes remain in event, so send another
       line starting from that point */
    const char *next =
        event + PI_MAX_LOGLEN-2 - (strpbrk( buff+2, PI_LOGSEP )-buff+1);

    while ( len >= PI_MAX_LOGLEN-1 ) {
        buff[PI_MAX_LOGLEN-1] = '+';	// insert continuation character

        PI_CALLMPI( MPI_Send( buff, PI_MAX_LOGLEN, MPI_CHAR,
                              thisproc.svc_flag[OLP_RANK], 0, PI_CommWorld ) )

        len = snprintf( buff, PI_MAX_LOGLEN-1, "%s", next );
        next = next + PI_MAX_LOGLEN-2;
    }
    buff[PI_MAX_LOGLEN-1] = ' ';    // indicate last line

    PI_CALLMPI( MPI_Send( buff, PI_MAX_LOGLEN, MPI_CHAR,
                          thisproc.svc_flag[OLP_RANK], 0, PI_CommWorld ) )
}

/* -------- Format String Parsing -------- */

/*!
********************************************************************************
Stores the first three alphanumeric chars of \p bytes into an integer. This
provides O(1) lookup time for the conversion specifiers. It is case sensitive.

\post Bits 0 - 23 of the return value contain the 3 chars.
*******************************************************************************/
static int32_t BytesToInt( const char *bytes )
{
    int32_t result = 0;
    int i = 0;
    while ( i < 3 && isalnum( bytes[ i ] ) ) {
        result |= ( bytes[ i ] & 0xff ) << ( 16 - i * 8 );
        i += 1;
    }
    return result;
}

/*!
********************************************************************************
Stores the first \p len chars of \p bytes into an integer. This provides O(1)
lookup time for the reduce operators. It is case sensitive.

\pre len should be in the range 1-3
\post Bits 0 - 23 of the return value contain the 1-3 chars.
*******************************************************************************/
static int32_t OpToInt( int len, const char *bytes )
{
    int32_t result = 0;
    int i = 0;
    while ( i < len ) {
        result |= ( bytes[ i ] & 0xff ) << ( 16 - i * 8 );
        i += 1;
    }
    return result;
}


/*!
********************************************************************************
Fills in \c rtti->type and \c rtti->cType given a conversion specification.

\return The number of characters that have been successfully processed.
\retval 0 indicates failure since conversion specifiers are at least one
char long.
*******************************************************************************/
static int LookupConversionSpec( const char *key, PI_MPI_RTTI *rtti )
{
    CTYPE cType = CTYPE_INVALID;
    MPI_Datatype mpiType = MPI_DATATYPE_NULL;
    int skip = 0;

// Easy-ish way to define a conversion specifier.
#define CONVERSION_SPEC( c1, c2, c3 ) \
    ( ( (c1) & 0xff ) << 16 | ( (c2) & 0xff ) << 8 | ( (c3) & 0xff ) )

    switch ( BytesToInt( key ) ) {
    case CONVERSION_SPEC( 'b', 0, 0 ):
        cType = CTYPE_BYTE;
        mpiType = MPI_BYTE;
        skip = 1;
        break;

    case CONVERSION_SPEC( 'c', 0, 0 ):
    case CONVERSION_SPEC( 's', 0, 0 ):
        cType = CTYPE_CHAR;
        mpiType = MPI_CHAR;
        skip = 1;
        break;

    case CONVERSION_SPEC( 'h', 'd', 0 ):
    case CONVERSION_SPEC( 'h', 'i', 0 ):
        cType = CTYPE_SHORT;
        mpiType = MPI_SHORT;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'd', 0, 0 ):
    case CONVERSION_SPEC( 'i', 0, 0 ):
        cType = CTYPE_INT;
        mpiType = MPI_INT;
        skip = 1;
        break;

    case CONVERSION_SPEC( 'l', 'd', 0 ):
    case CONVERSION_SPEC( 'l', 'i', 0 ):
        cType = CTYPE_LONG;
        mpiType = MPI_LONG;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'l', 'l', 'd' ):
    case CONVERSION_SPEC( 'l', 'l', 'i' ):
        cType = CTYPE_LONG_LONG;
        mpiType = MPI_LONG_LONG;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'h', 'h', 'u' ):
        cType = CTYPE_UNSIGNED_CHAR;
        mpiType = MPI_UNSIGNED_CHAR;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'h', 'u', 0 ):
        cType = CTYPE_UNSIGNED_SHORT;
        mpiType = MPI_UNSIGNED_SHORT;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'u', 0, 0 ):
        cType = CTYPE_UNSIGNED;
        mpiType = MPI_UNSIGNED;
        skip = 1;
        break;

    case CONVERSION_SPEC( 'l', 'u', 0 ):
        cType = CTYPE_UNSIGNED_LONG;
        mpiType = MPI_UNSIGNED_LONG;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'l', 'l', 'u' ):
        cType = CTYPE_UNSIGNED_LONG_LONG;
        mpiType = MPI_UNSIGNED_LONG_LONG;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'f', 0, 0 ):
        cType = CTYPE_FLOAT;
        mpiType = MPI_FLOAT;
        skip = 1;
        break;

    case CONVERSION_SPEC( 'l', 'f', 0 ):
        cType = CTYPE_DOUBLE;
        mpiType = MPI_DOUBLE;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'L', 'f', 0 ):
        cType = CTYPE_LONG_DOUBLE;
        mpiType = MPI_LONG_DOUBLE;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'm', 0, 0 ):
        cType = CTYPE_USER_DEFINED;
        /* Let the caller fill in the MPI_Datatype */
        skip = 1;
        break;

        // Fortran types start with 'F' (suppress if symbols not defined)

#ifndef MPID_NO_FORTRAN

        /* HP MPI only supplies MPI_xxx symbols for a handful of "C usable"
           Fortran types. Follow their model in mpi.h to define the missing
           symbols. */

#ifdef HP_MPI
#define MPI_INTEGER1             ((MPI_Datatype) MPI_Type_f2c(MPIF_INTEGER1))
#define MPI_INTEGER2             ((MPI_Datatype) MPI_Type_f2c(MPIF_INTEGER2))
#define MPI_INTEGER4             ((MPI_Datatype) MPI_Type_f2c(MPIF_INTEGER4))
#define MPI_REAL4                ((MPI_Datatype) MPI_Type_f2c(MPIF_REAL4))
#define MPI_REAL8                ((MPI_Datatype) MPI_Type_f2c(MPIF_REAL8))


        /* There are still two "newly added datatypes" only found in mpif.h.
           Ideally, these should be extracted from mpif.h by "configure" when
           Pilot is installed with a given MPI lib, in case they change.

               parameter (MPI_INTEGER8=36)
               parameter (MPI_REAL16=37)
        */

#define MPI_INTEGER8             ((MPI_Datatype) MPI_Type_f2c(36))
#define MPI_REAL16               ((MPI_Datatype) MPI_Type_f2c(37))
#endif

    case CONVERSION_SPEC( 'F', 'c', 0 ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_CHARACTER;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'F', 'i', 0 ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_INTEGER;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'F', 'i', '1' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_INTEGER1;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'F', 'i', '2' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_INTEGER2;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'F', 'i', '4' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_INTEGER4;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'F', 'i', '8' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_INTEGER8;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'F', 'r', 0 ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_REAL;
        skip = 2;
        break;

    case CONVERSION_SPEC( 'F', 'r', '4' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_REAL4;
        skip = 3;
        break;

    case CONVERSION_SPEC( 'F', 'r', '8' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_REAL8;
        skip = 3;
        break;

        /* On some platforms using OpenMPI, this type is sometimes not
         * supported, and then the compile will fail. Only make this format
         * available if the symbol doesn't exist (not OpenMPI) or it's true.
         */
#if !defined(OMPI_HAVE_FORTRAN_REAL16) || OMPI_HAVE_FORTRAN_REAL16
    case CONVERSION_SPEC( 'F', 'r', '6' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_REAL16;
        skip = 3;
        break;
#endif

    case CONVERSION_SPEC( 'F', 'd', 'p' ):
        cType = CTYPE_FORTRAN;
        mpiType = MPI_DOUBLE_PRECISION;
        skip = 3;
        break;
#endif

    default:
        return 0;
    }

    if ( rtti ) {
        rtti->cType = cType;
        rtti->type = mpiType;
    }

    return skip;
}

/*!
********************************************************************************
Fills in \c rtti->op given a reduce operator in the first \p len bytes of \p key.
If op is MPI_OP_NULL but the return value is non-0, then the caller should obtain
a user-defined operator.

\return The number of characters that have been successfully processed.
\retval 0 indicates failure since reduce operators are at least one
char long.
*******************************************************************************/
static int LookupReduceOp( int len, const char *key, PI_MPI_RTTI *rtti )
{
    MPI_Op mpiOp = MPI_OP_NULL;
    int skip = 0;

// Easy-ish way to define an operator.
#define REDUCE_OP CONVERSION_SPEC

    switch ( OpToInt( len, key ) ) {
    case REDUCE_OP( 'm', 'i', 'n' ):
        mpiOp = MPI_MIN;
        skip = 3;
        break;

    case REDUCE_OP( 'm', 'a', 'x' ):
        mpiOp = MPI_MAX;
        skip = 3;
        break;

    case REDUCE_OP( '+', 0, 0 ):
        mpiOp = MPI_SUM;
        skip = 1;
        break;

    case REDUCE_OP( '*', 0, 0 ):
        mpiOp = MPI_PROD;
        skip = 1;
        break;

    case REDUCE_OP( '&', '&', 0 ):
        mpiOp = MPI_LAND;
        skip = 2;
        break;

    case REDUCE_OP( '|', '|', 0 ):
        mpiOp = MPI_LOR;
        skip = 2;
        break;

    case REDUCE_OP( '^', '^', 0 ):
        mpiOp = MPI_LXOR;
        skip = 2;
        break;

    case REDUCE_OP( '&', 0, 0 ):
        mpiOp = MPI_BAND;
        skip = 1;
        break;

    case REDUCE_OP( '|', 0, 0 ):
        mpiOp = MPI_BOR;
        skip = 1;
        break;

    case REDUCE_OP( '^', 0, 0 ):
        mpiOp = MPI_BXOR;
        skip = 1;
        break;

    case REDUCE_OP( 'm', 'o', 'p' ):
        /* let caller fill in user-defined operator */
        skip = 3;
        break;

    default:
        return 0;
    }

    if ( rtti ) {
        rtti->op = mpiOp;
    }

    return skip;
}


/*!
********************************************************************************
Checks a supposed pointer to see which segment of process memory it likely belongs
to.  Purpose is to validate pointers for reading anything and for writing arrays.
Generally speaking, false positives (real pointers judged bogus) should not occur,
unless this is run on an OS that divvies up memory in an unexpected fashion.  But
false negatives (data that looks like pointers) can occur.  That's OK because it
means that (1) we won't abort the program for a "bad" pointer that isn't, and (2)
we just won't catch every mistake the user makes (of substituting values for
locations).

To be safe, a return of 0 or 1 should be considered invalid, because most compilers
are not going to put even read-only literals in the text segment.

\param ptr Pointer to be checked
\retval 0 Cannot be classified or NULL, likely a bogus pointer
\retval 1 Within the text segment (program code, often write-protected)
\retval 2 Within the (un)initialized data segment (static/global variables)
\retval 3 On the stack (local variables)
\retval 4 On the heap (dynamically allocated storage)
*******************************************************************************/
static int CheckPointer( void *ptr )
{
    int mystackvar;

    /* Here's the invariant:
     *		0 < etext < end < break < mystackvar < ArgvCopy < top of memory
     *
     * Since etext and end are established by the linker/loader, the break
     * is managed by the runtime library, and mystackvar is positioned by
     * compiled code, the only one that can go astray is ArgvCopy.  That would
     * happen if PI_Configure was called with a fake argc/argv for some reason.
     *
     * So if ArgvCopy is out of order, we'll just ignore it.
     */

    /* Treat NULL as a special case, otherwise it will likely look like a text
     * address; it will never be valid as a read/write arg.
     */
    if ( ptr == NULL ) return 0;

    /* Anything below etext is program code, or read-only data stored there.
     */
    if ( ptr <= (void*)&etext ) return 1;

    /* A static/global variable is in low memory, and should be:
     *  > etext (end of text segment)
     *  <= end (end of data segment)
     */
    if ( ptr > (void*)&etext && ptr <= (void*)&end ) return 2;

    /* The stack grows down from high memory, so caller's stack variable should be:
     * 	> a variable on my own stack frame
     *  <= argv location (OS puts the args and env vars just above the stack)
     */
    if ( ptr > (void*)&mystackvar ) {
        if ( (void*)&mystackvar < ArgvCopy && ptr <= ArgvCopy ) return 3;
        else return 3;
    }

    /* The heap grows up, so a malloc'd variable should be:
     *  > end (end of data segment)
     *  <= program break (current top of heap, obtained from sbrk())
     */
    if ( ptr >= (void*)&end && ptr <= (void*)sbrk(0) ) return 4;


    return 0;	// looks like a bogus address to me!
}


/*!
********************************************************************************
Computes the "signature" of a format string, so that a reader's string can be
compared with a writer's to verify that they match.  The simple compaction
algorithm reduces a parsed format array to a single 32-bit quantity, which is
cheaper to send in a message and easy to compare than working with a textual
format string or its parsed array form.

Because of compaction, aliasing of different formats to the same number is to
be expected.  Nonetheless, this should be rare because, first, users tend to
code a single item in a format string, which should produce a unique number, and
second, the compaction algorithm is more complex than a simple checksum but still
fast to compute.

Each meta array element represents one MPI message.  The compaction algorithm takes the
following into account in deciding which elements must be included in computing the
signature:
1) Scalar formats match iff their datatypes match.
2) Array formats match iff their datatypes and lengths match.
3) Reductions also require a matching reduce operator.
4) Array formats using the runtime variable length feature (^ flag or %s) need only
   match their datatypes.

Re 4), such formats are comprised of 2 successive meta elements.  The 1st (length)
element can be ignored, and the datatype will be extracted from the 2nd element.

\param meta  The array of parsed arguments filled in by ParseFormatString().
\param items  Number of format elements in the meta array.
\return  The signature of the format array reduced to a single number.
*******************************************************************************/
static uint32_t FormatSignature( PI_MPI_RTTI meta[], int items )
{
    int i;
    uint32_t sig = 0;	// signature to be returned

    for ( i=0; i<items; i++ ) {
        // these are the fields we're filling in...
        int length=0, varflag=0, redopflag=0, type;

        // if "sendCount" flag is set, we only care about the type in the *next* item
        if ( meta[i].sendCount ) {
            varflag = 1;
            i++;			// advance item index!
        }

        // general case
        else {
            length = meta[i].count;

            /* A reduce op is represented by a field of type MPI_Op. This is an
             * opaque datatype, meaning that it is almost surely implemented as a
             * pointer (probably to a structure). In order to support a user-defined
             * operation, MPI_Op_create most likely allocates a structure on the heap,
             * whereas builtin operators likely reside in static storage.
             *
             * For the latter, we can calculate a numeric code for a given operator
             * by taking the difference of its pointer value and that of MPI_SUM.
             * This number is going to be the same in any MPI process, therefore it
             * provides a way of verifying whether the writers and reducer are
             * using the same operation.
             *
             * For user-defined operators -- which we can recognize if the pointer
             * value lies in the heap -- this trick isn't going to work because the
             * heap address will be different on every MPI process. In that case,
             * assign an arbitrary code, and hope that the writers and reducer are
             * not using different user-defined operators (which seems most unlikely).
             *
             * Once a numeric code has been created, add that to the length field in
             * order to record it in the format signature.
             */
            if ( meta[i].op != MPI_OP_NULL ) {
                redopflag = 1;

                uintptr_t redop = (uintptr_t)meta[i].op,
                          opbase = (uintptr_t)MPI_SUM;

                if ( CheckPointer( (void*)redop ) < 3 )		// in static storage
                    length += redop > opbase ? redop-opbase : opbase-redop;
                else
                    length += 999;	// code chosen to represent user-defined op
            }
        }

        type = meta[i].cType;

        // allow 25 bits for the count, 1 flag each for var. length & reduce op
        // and finally 5 bits for datatype
        uint32_t item = (length & 0x1ffffff) << 7
                        | varflag << 6
                        | redopflag << 5
                        | (type & 0x1f);

        // combine with earlier sig by shifting it 3 bits and XORing
        sig = (sig << 3) ^ item;
    }

    return sig;
}


/*!
********************************************************************************
Parse a printf like format string into data which describes MPI data. This function
aborts upon encountering a format error, unless PI_OnErrorReturn is set.

\param valsOrLocs  Whether the va_list should be interpreted as a list of
values (e.g., PI_Write) or locations (e.g., PI_Read). Locations are demanded for
certain collective output functions that draw from arrays (PI_Scatter). If values
are allowed, locations can still be distinguished by coding a length.
\param meta  An array of size PI_MAX_FORMATLEN to hold the parsed arguments.
\param fmt  Printf like format to be parsed.
\param ap  The va_list to read the arguments from. It is expected that the first
argument is an integer giving the number of remaining args in the va_list.
This function uses the va_arg macro, so the value of `ap` is undefined after the call.
This function does not call va_end on ap. See stdarg(3).

\return The number of MPI messages that fmt requires to be sent or received, i.e.,
the number of meta elements filled (which is normally the number of terms
parsed, but can be greater if the ^ flag or %s datatype is used) or -1 when
an invalid format string is encountered.
*******************************************************************************/
static int ParseFormatString( IO_CONTEXT valsOrLocs, PI_MPI_RTTI meta[],
                              const char *fmt, va_list ap )
{
    const char *s = fmt;
    int metaIndex, nargs;

    PI_ON_ERROR_RETURN( -1 );
    PI_ASSERT( , fmt != NULL, PI_NULL_FORMAT );

    /* Grab the no. of args that the caller supplied.  We will count these down
     * to verify that the number of formats matches the args.  The check is done
     * via PI_ASSERT, which should find that nargs-- > 0, until the formats are
     * exhausted.
     */
    nargs = va_arg( ap, int );

    /* This loop runs until the format string has been fully parsed.
     * It normally generates one meta element per format type code.
     * However, formats that send an extra message with count info (^ flag
     * and %s type) generate two elements. In that case, the loop index
     * will be incremented inside the loop. In the end, there will be one
     * meta element per message that needs to be sent or received.
     */
    for ( metaIndex = 0; metaIndex < PI_MAX_FORMATLEN; metaIndex++ ) {
        PI_MPI_RTTI *rtti = &meta[ metaIndex ];
        rtti->sendCount = 0;		// assume no need to send count (=array size)
        rtti->op = MPI_OP_NULL;		// assume no reduce op
        int count = -1;			// -1 = haven't found count specified (yet)

        /* Skip whitespace in fmt */
        while ( isspace(*s) ) s++;

        if ( *s == '\0' )
            break;

        /* The next char must mark the start of a conversion specification. */
        PI_ASSERT( , *s == '%', PI_FORMAT_INVALID );

        s++;
        PI_ASSERT( , *s != '\0', PI_FORMAT_INVALID );

        /* Parse optional reduce operation */
        char *slash = strchr( s , '/' );	// look for slash
        if ( slash ) {
            int oplen = slash - s;		// op should be 1-3 chars
            PI_ASSERT( , oplen>=1 || oplen<=3, PI_FORMAT_INVALID );

            /* Figure out which MPI op to use */
            int skip = LookupReduceOp( oplen, s, rtti );
            PI_ASSERT( , skip > 0, PI_FORMAT_INVALID );

            /* Obtain user-defined operator from next arg */
            if ( rtti->op == MPI_OP_NULL ) {
                PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
                rtti->op = va_arg( ap, MPI_Op );
            }
            s = slash+1;
        }

        /* Parse optional array size */
        if ( isdigit(*s) ) {
            /* This should be the 1st digit seen */
            PI_ASSERT( , count == -1, PI_FORMAT_INVALID );
            count = 0;
            do {
                count = count * 10 + ( *s++ ) - '0';
            } while ( isdigit(*s) );
            /* Digits must be followed by a type */
            PI_ASSERT( , *s != '\0', PI_FORMAT_INVALID );

            /* Disallow "%1" since 1 (one) looks too similar to l (ell).
               Also disallow "%0" as it has no meaning. */
            PI_ASSERT( , count > 1, PI_ARRAY_LENGTH );
        }

        /* '*' specifies array size supplied in int arg */
        if ( *s == '*' ) {
            /* Make sure the count has not already been specified */
            PI_ASSERT( , count == -1, PI_ARRAY_LENGTH );
            /* Grab next arg for size */
            PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
            count = va_arg( ap, int );
            PI_ASSERT( , count > 0, PI_ARRAY_LENGTH );
            s++;
            PI_ASSERT( , *s != '\0', PI_FORMAT_INVALID );
        }

        /* Handle '^' flag and 's' string datatype:
         * Both specify automatic buffer allocation on read end and generate an extra
         * array length message. The difference is that 's' calculates the length on
         * writing, and does not store it in an arg on reading.
         */
        if ( *s == '^' || *s == 's' ) {
            /* Make sure we're not into a reduction; ^ would not make sense */
            PI_ASSERT( , rtti->op == MPI_OP_NULL, PI_FORMAT_INVALID );
            /* Make sure the count has not been specified */
            PI_ASSERT( , count == -1, PI_ARRAY_LENGTH );
            rtti->count = 1;	// refers to length of array size msg (1 x int)
            rtti->type = MPI_INT;
            rtti->cType = CTYPE_INT;
            rtti->sendCount = 1;	// flag that count has to be sent from writer            }

            /* Reading: 1st element inputs integer array size */
            if ( valsOrLocs == IO_CONTEXT_LOCS ) {
                /* general case: Grab next arg as location to store array size */
                if ( *s == '^' ) {
                    PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
                    rtti->data.address = va_arg( ap, void* );
                    rtti->buf = rtti->data.address;
                }
                /* 's' case: Store the length in the parse element itself */
                else {
                    rtti->buf = &rtti->data.d;
                }
            }

            /* Writing: 1st element sends integer array size from supplied variable */
            else {
                /* general case: Grab next arg as the array size */
                if ( *s == '^' ) {
                    PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
                    count = va_arg( ap, int );
                    PI_ASSERT( , count > 0, PI_ARRAY_LENGTH );
                }
                /* 's' case: Peek at next arg's strlen */
                else {
                    /* first, copy the arg list so we can "peek" not consume next arg */
                    PI_ASSERT( LEVEL(1), nargs > 0, PI_FORMAT_ARGS );
                    va_list temp;
                    va_copy( temp, ap );
                    count = 1 + strlen( (char *)va_arg( temp, char* ) ); // +1 for NUL term
                    va_end( temp );
                }
                rtti->data.d = count;
                rtti->buf = &rtti->data.d;
            }

            /* advance ^ parsing to type code (for 's' case we let it scan the 's') */
            if ( *s == '^' ) {
                s++;
                PI_ASSERT( , *s != '\0', PI_FORMAT_INVALID );
            }

            /* then start another element */
            metaIndex++;
            PI_ASSERT( , metaIndex < PI_MAX_FORMATLEN, PI_FORMAT_INVALID );
            rtti = &meta[ metaIndex ];
            rtti->sendCount = 0;
            rtti->op = MPI_OP_NULL;
        }

        /* Figure out which MPI type to use */
        int skip = LookupConversionSpec( s, rtti );
        PI_ASSERT( , skip > 0, PI_FORMAT_INVALID );

        /* Here we may want to verify that a reduce operation is compatible
           with the MPI type, but let's see if/how MPI detects problems. */

        /* Set `rtti->buf` to point to the appropriate data. */
        if ( valsOrLocs == IO_CONTEXT_LOCS || count >= 1 ) {
            if ( count <= 0 ) {
                // This is a Read into a Scalar.
                rtti->count = 1;
            }
            else rtti->count = count;

            /* For user defined types, collect the datatype from the user */
            if ( rtti->cType == CTYPE_USER_DEFINED ) {
                PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
                rtti->type = va_arg( ap, MPI_Datatype );
            }

            PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
            rtti->data.address = va_arg( ap, void* );
            rtti->buf = rtti->data.address;
            PI_ASSERT( LEVEL(3), CheckPointer( rtti->buf ) > 1, PI_BOGUS_POINTER_ARG );
        }
        else if ( valsOrLocs == IO_CONTEXT_VALS ) {
            /* Writing a single scalar => copy it into a buffer.
             *
             * NOTE! Cases with casting, (foo)va_arg(ap,bar), are because C's default
             * argument promotion, which happens in the presence of "...", will have
             * promoted the user's arg actually of type foo to be stored as a type bar.
             * Thus, we need to pull it with va_arg as if it's a bar type, but then
             * we "demote" it to its original foo type for sending in a message.
             *
             * The default promotions are:
             * - char & short => int
             * - unsigned char & short => unsigned int
             * - float => double
             */
            PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
            rtti->count = 1;
            switch ( rtti->cType ) {
            case CTYPE_CHAR:
                rtti->data.c = ( char )va_arg( ap, int );
                rtti->buf = &rtti->data.c;
                break;
            case CTYPE_SHORT:
                rtti->data.h = ( short )va_arg( ap, int );
                rtti->buf = &rtti->data.h;
                break;
            case CTYPE_INT:
                rtti->data.d = va_arg( ap, int );
                rtti->buf = &rtti->data.d;
                break;
            case CTYPE_LONG:
                rtti->data.ld = va_arg( ap, long int );
                rtti->buf = &rtti->data.ld;
                break;
            case CTYPE_UNSIGNED_CHAR:
                rtti->data.hhu = ( unsigned char )va_arg( ap, unsigned int );
                rtti->buf = &rtti->data.hhu;
                break;
            case CTYPE_UNSIGNED_SHORT:
                rtti->data.hu = ( unsigned short )va_arg( ap, unsigned int );
                rtti->buf = &rtti->data.hu;
                break;
            case CTYPE_UNSIGNED_LONG:
                rtti->data.lu = va_arg( ap, unsigned long );
                rtti->buf = &rtti->data.lu;
                break;
            case CTYPE_UNSIGNED:
                rtti->data.ld = va_arg( ap, unsigned int );
                rtti->buf = &rtti->data.ld;
                break;
            case CTYPE_FLOAT:
                rtti->data.f = ( float )va_arg( ap, double );
                rtti->buf = &rtti->data.f;
                break;
            case CTYPE_DOUBLE:
                rtti->data.lf = va_arg( ap, double );
                rtti->buf = &rtti->data.lf;
                break;
            case CTYPE_LONG_DOUBLE:
                rtti->data.llf = va_arg( ap, long double );
                rtti->buf = &rtti->data.llf;
                break;
            case CTYPE_BYTE:
                rtti->data.b = ( unsigned char )va_arg( ap, unsigned int );
                rtti->buf = &rtti->data.b;
                break;
            case CTYPE_LONG_LONG:
                rtti->data.lld = va_arg( ap, long long int );
                rtti->buf = &rtti->data.lld;
                break;
            case CTYPE_UNSIGNED_LONG_LONG:
                rtti->data.llu = va_arg( ap, unsigned long long int );
                rtti->buf = &rtti->data.llu;
                break;
            case CTYPE_USER_DEFINED:
                rtti->type = va_arg( ap, MPI_Datatype );
                PI_ASSERT( LEVEL(1), nargs-- > 0, PI_FORMAT_ARGS );
                rtti->data.address = va_arg( ap, void* );
                rtti->buf = rtti->data.address;
                PI_ASSERT( LEVEL(3), CheckPointer( rtti->buf ) > 1, PI_BOGUS_POINTER_ARG );
                break;
            default:
                PI_ASSERT( , 0, PI_SYSTEM_ERROR );
            }
        }
        else {
            PI_ASSERT( , 0, PI_SYSTEM_ERROR );
        }

        /* Move onto the next conversion specifier. */
        s += skip;
    }

    /* The format string is nothing but whitespace. */
    PI_ASSERT( , metaIndex != 0, PI_FORMAT_INVALID );
    /* The format string contains too many arguments. */
    PI_ASSERT( , metaIndex != PI_MAX_FORMATLEN, PI_FORMAT_ARGS );

    /* End of format string -- there should now be no more args */
    PI_ASSERT( LEVEL(1), nargs == 0, PI_FORMAT_ARGS )

    /* Keep this code available for checking calculated signatures
     *
    if ( metaIndex > 0 ) {
    uint32_t sig = FormatSignature( meta, metaIndex );
    printf( "@@@ %s '%s' = %u 0%o: %d, %c%c, %d\n",
        (valsOrLocs == IO_CONTEXT_LOCS ? "Reading <<<" : "Writing >>>"),
        fmt,
        (unsigned)sig,
        (unsigned)sig,
        (unsigned)sig >> 7,
        (sig >> 6) & 1 ? '^' : '_',
        (sig >> 5) & 1 ? '/' : '_',
        (unsigned)sig & 0x1f );
    }*/

    return metaIndex;
}
