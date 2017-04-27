/***************************************************************************
 * Copyright (c) 2008-2017 University of Guelph.
 *                         All rights reserved.
 *
 * This file is part of the Pilot software package.  For license
 * information, see the LICENSE file in the top level directory of the
 * Pilot source distribution.
 **************************************************************************/

/*!
********************************************************************************
\file pilot_private.h
\brief Header file for functions used by the pilot library.
\author John Douglas Carter
\author Sid Bao

Provides utility types, macros and prototypes for Pilot. Symbols that are only
defined here and not also in pilot.h do not pollute the global namespace; they
are only used in Pilot internal code. Hence, the PI_ prefix is optional.
*******************************************************************************/

#ifndef PILOT_PRIVATE_H
#define PILOT_PRIVATE_H

/*! Change this for a new version. */
#define PI_VERSION "3.2"

/*! Printed on welcome banner. */
#define PI_HELLO "Pilot " PI_VERSION " for MPI - University of Guelph"

/*! Printed to left of Pilot output so it's not confused with MPI output. */
#define PI_BORDER "[PILOT] "

/*** Declarations for PointerCheck() function ***
 * These are made here in .h file to keep Intel compiler quiet.  These
 * symbols are filled in by the linker/loader.  #including unistd.h is done
 * here to get the _BSD_SOURCE feature test macro defined before it can get
 * defined in another .h file.  On some platforms, you can get sbrk() without
 * this macro.
 *
 * In addition, _BSD_SOURCE is now "deprecated" since glibc 2.19.  Instead of
 * just changing it to the preferred _DEFAULT_SOURCE, advice is to define both:
 * http://stackoverflow.com/questions/29201515/what-does-d-default-source-do#29201732
 */
extern char etext,	// end of program text segment
	    end;	// end of uninitialized data segment (BSS)
	    
#define _BSD_SOURCE	// may be needed to get sbrk()
#define _DEFAULT_SOURCE
#include <unistd.h>	    

#include "pilot_limits.h"
#include "pilot_log_colors.h"
#include <mpi.h>

/*!
********************************************************************************
\brief Signature for work functions.

Functions to be assigned to processes must match this prototype.
*******************************************************************************/
typedef int(*PI_WORK_FUNC)(int,void*);
typedef int(*PI_WORK_FTN)(int*,void**);	// Fortran func (call by ref)

/*! Maximum length of each line in log messages (longer lines are wrapped). */
#define PI_MAX_LOGLEN 80

/*! Character (in double quotes) used to separate fields on a log line. */
#define PI_LOGSEP "\t"


/*** Magic Numbers used to validate data structures with ISVALID ***/

#define PI_PROC 899503453
#define PI_CHAN 937927385
#define PI_BUND 152536731

/*** Pilot macros for error checking ***
 These are for use by API functions and those called by them, chiefly to
 validate preconditions. They should NOT be used by autonomous internal
 modules of Pilot, e.g, online processes/threads.
*/

/*! Use at start of function to specify an error return value appropriate for function's type */
#define PI_ON_ERROR_RETURN( return_value ) if(0){ error_return: return return_value; }

/*! Use to check a structure's magic no. provided the pointer is non-NULL */
#define ISVALID( magicnum, ptr ) ( (ptr) && (ptr)->magic==magicnum )

/*! Use as first arg. to PI_ASSERT if check only applies at level n or above */
#define LEVEL( n ) (PI_CheckLevel >= n) &&


/*  PI_ASSERT is intended for use by the public API functions of Pilot, since
    they return to their caller.  It can be used by subfunctions if the caller
    checks their return and relays it to the API's caller.  It is NOT for use by
    any online processes; instead they can call PI_Abort.

    Perform error check in style of assert(); if level is non-blank, it acts as a guard;
    if assertion fails (true_exp is false), PI_Abort is normally called with file/line
    recorded in PI_Caller*, plus immediate file/line if system error.  But if
    PI_OnErrorReturn is non-zero, the error return set by PI_ON_ERROR_RETURN will be
    executed after setting PI_Errno.  If you code PI_ASSERT but not PI_ON_ERROR_RETURN,
    you'll get a compiler error re "error_return".

    true_exp can have side effects provided level is blank (otherwise, while the expression
    is always compiled (unlike C assertions), it may not be evaluated!)

    PI_STR and PI_ASSERT2 are just hacks to get the immediate line no. into a string.
*/
#define PI_STR(x) #x
#define PI_ASSERT( level, true_exp, err_code ) \
	PI_ASSERT2( level, true_exp, err_code, __LINE__ )

#define PI_ASSERT2( level, true_exp, err_code, thisline ) \
	if ( level !( true_exp ) ) { \
	    if ( PI_OnErrorReturn ) { PI_Errno = err_code; goto error_return; } \
	    else PI_Abort( err_code, \
		( err_code==PI_SYSTEM_ERROR ? (" at " __FILE__ ":" PI_STR(thisline)) : "" ), \
		PI_CallerFile, \
		PI_CallerLine ); }

/*!
********************************************************************************
PI_OLP_ASSERT is intended for use by online processes. It simply tests the
condition and calls PI_Abort if the test fails. true_exp can have side
effects.
*******************************************************************************/
#define PI_OLP_ASSERT( true_exp, err_code ) \
	if ( !( true_exp ) ) PI_Abort( err_code, "", __FILE__, __LINE__ );


/*! Wrapper for MPI library calls that records location of call in library source */
#define PI_CALLMPI( stmt ) \
	(MPICallLine = __LINE__), stmt;

/*! Macro for logging calls if enabled */
#define LOGCALL( code, chanbund, format, partno, parts, arg ) \
    if ( thisproc.svc_flag[LOG_CALLS] ) { \
        char buff[256]; \
        if ( parts > 1 ) \
            snprintf( buff, sizeof(buff), "%s" PI_LOGSEP "%d" PI_LOGSEP "%.12s:%d" PI_LOGSEP "%s (part %d/%d)", \
                (code), (chanbund), (PI_CallerFile), (PI_CallerLine), (format), (partno), (parts) ); \
        else snprintf( buff, sizeof(buff), "%s" PI_LOGSEP "%d" PI_LOGSEP "%.12s:%d" PI_LOGSEP "%s", \
                (code), (chanbund), (PI_CallerFile), (PI_CallerLine), (format) ); \
        LogEvent( CALLS, interpArg( buff, sizeof(buff)-strlen(buff)-1, (code), (arg) ) ); \
    }

typedef struct PI_PROCESS PI_PROCESS;		// forward declarations
typedef struct PI_CHANNEL PI_CHANNEL;
typedef struct PI_BUNDLE PI_BUNDLE;

/*!
********************************************************************************
\brief Type used for Pilot channels.

Contains everything associated with a channel, and forms one entry in
PI_PROCENVT's channel table.
*******************************************************************************/
struct PI_CHANNEL
{
    int chan_id;	/*!< Identifier for this channel, starts from 1. */
    char name[PI_MAX_NAMELEN];	/*!< Friendly name of the channel. */

    int producer;	/*!< Rank of the write-end of channel. */
    int consumer;	/*!< Rank of the read-end of channel. */

    int chan_tag;	/*!< MPI tag of the channel, starts as chan_id, may be changed if part of Selector bundle */
    PI_BUNDLE *bundle;	/*!< Associated collective bundle, or NULL */

    int magic;		/*!< Fill in with PI_CHAN */
};

/*!
********************************************************************************
\brief Type used for Pilot processes.

Contains everything associated with a process, and forms one entry in
PI_PROCENVT's process table.
*******************************************************************************/
struct PI_PROCESS
{
    int rank;	/*!< Rank of the MPI process assigned to this Pilot process, starts from 0. */
    char name[PI_MAX_NAMELEN];	/*!< Friendly name for this process. */

    PI_WORK_FUNC run;	/*!< Pointer to the function associated with this process. */
    int call;		/*!< Style of func call: 0=C, 1=Fortran */
    int argument;	/*!< Numeric associted with this process -- used for algorithms. */
    void *argument2;  	/*!< Void pointer which *may* be used by this process */

    int magic;		/*!< Fill in with PI_PROC */
};

/*!
********************************************************************************
\brief Type used for a Pilot grouping of channels.

Associates a number of channels for select/collective operations.  Each
bundle forms one entry in PI_PROCENVT's bundle table.
*******************************************************************************/
struct PI_BUNDLE
{
    int bund_id;	/*!< Identifier for this bundle, starts from 1. */
    char name[PI_MAX_NAMELEN];	/*!< Friendly name for this bundle. */

    int usage;		/*!< Fixed usage of this bundle (see enum PI_BUNUSE). */
    enum { FROM, TO } narrow_end;	/*!< Narrow end's role */
    int size;		/*!< Number of channels in this bundle. */
    PI_CHANNEL **channels;	/*!< Array of channels. */
    MPI_Comm comm;   	/*!< Communicator associated with this bundle */

    int magic;		/*!< Fill in with PI_BUND */
};

/*!
********************************************************************************
\struct PI_PROCENVT
\brief Type used to hold all data that an individual process 'knows about'.

The environment contains all pointers, and bookkeeping data for every process.
Each process maintains its own environment, stored as a static variable.
*******************************************************************************/
enum {LOGGING=0, LOG_TABLES, LOG_CALLS, LOG_STATS, LOG_MPE,
	OLP_LOGFILE, OLP_DEADLOCK, OLP_RANK,
	SVC_END}; /*!< Flag indexes for use with svc_flag array */
typedef unsigned char Flag_t;

typedef struct
{
    enum {PREINIT, CONFIG, RUNNING, POSTRUN} phase; /*!< Phase of application life cycle. */

    enum {LOG_WRITE=0, LOG_READ, LOG_CONFIGURE, LOG_WRITE_MSG, LOG_CHANNEL, LOG_COMPUTE,
        LOG_SELECT, LOG_BROADCAST, LOG_BROADCAST_MSG, LOG_GATHER, LOG_GATHER_MSG,
        LOG_SCATTER, LOG_SCATTER_MSG, LOG_REDUCE, LOG_REDUCE_MSG, LOG_STARTTIME,
        LOG_ENDTIME, LOG_TRYSELECT, LOG_CHANNELHASDATA, LOG_LOG, LOG_LAST} mpe_offset;
        /*!< Offset of event type in mpe_event/se arrays (length=LOG_LAST). */

    int worldsize;  	/*!< Number of processes allocated by MPI. */
    int rank;	/*!< The rank of process that this environment belongs to. */
    int i;

    /*!< Array of service flags (result of command-line options)
         The flag for service LOG_STATS is found in svc_flag[LOG_STATS], etc.
         Index OLP_RANK has the actual MPI rank no. of the online process. */
    Flag_t svc_flag[SVC_END];
          
    int mpe_eventse[LOG_LAST][2];    /*!< The start and end point for each event e.g. "event1a" and "event1b" as original. */
    
    int mpe_event[LOG_LAST];    /*!< The indices of events e.g. "event1a" and "event2" as original. */

    int allocated_processes;	/*!< Number of processes that have been created. */
    PI_PROCESS *processes;	/*!< Table of PI_PROCESS structs, with fixed no.
				     of rows = worldsize; indexed by rank. */

    int allocated_channels;	/*!< Number of channels that have been created. */
    PI_CHANNEL **channels;	/*!< Table of PI_CHANNEL* pointers, indexed by ID-1. */

    int allocated_bundles;   	/*!< Number of bundles that have been created. */
    PI_BUNDLE **bundles;  	/*!< Table of PI_BUNDLE* pointers, with fixed no.
				    of rows = PI_MAX_BUNDLES; indexed by ID-1. */

    double start_time;		/*!< For use by PI_Start/EndTime */
} PI_PROCENVT;


/*!
********************************************************************************
\enum CTYPE
\brief C datatypes that can be specified in format strings

Use this enum to help mapping between C datatypes and MPI datatypes.
MPI datatypes are not guaranteed to be integer constants. Note that since
Fortran Read/Write is always from/to an address, actual data values are
never supplied in the arg list and therefore no type-specific processesing
is ever needed.
*******************************************************************************/
typedef enum {
    CTYPE_INVALID = -1,
    CTYPE_CHAR,
    CTYPE_SHORT,
    CTYPE_INT,
    CTYPE_LONG,
    CTYPE_UNSIGNED_CHAR,
    CTYPE_UNSIGNED_SHORT,
    CTYPE_UNSIGNED_LONG,
    CTYPE_UNSIGNED,
    CTYPE_FLOAT,
    CTYPE_DOUBLE,
    CTYPE_LONG_DOUBLE,
    CTYPE_BYTE,
    CTYPE_LONG_LONG,
    CTYPE_UNSIGNED_LONG_LONG,
    CTYPE_FORTRAN,
    CTYPE_USER_DEFINED
} CTYPE;

/*!
********************************************************************************
\enum IO_CONTEXT
\brief Tells ParseFormatString whether an argument list is to be parsed as values or locations.
*******************************************************************************/
typedef enum {
    IO_CONTEXT_VALS,
    IO_CONTEXT_LOCS
} IO_CONTEXT;

/*!
********************************************************************************
\struct PI_MPI_RTTI
\brief Runtime Type Information parsed from a read/write format string.

ParseFormatString fills in an array of PI_MPI_RTTI's so that the data can
be used in calls to MPI_Send, MPI_Recv, etc. Each struct instance corresponds
to one MPI message.
*******************************************************************************/
typedef struct {
/* public: */
    void* buf;  /*!< Pointer to user data = MPI's "buf" argument. */
    int count;  /*!< Number of elements to send = MPI's "count" argument. */
    int sendCount; /*!< True if count will be sent in a separate message. */
    MPI_Datatype type;  /*!< The MPI datatype that `buf` points to = MPI's "datatype" argument. */
    MPI_Op op;	/*!< The reduce operation, if any, else MPI_OP_NULL. */

/* private: */
    CTYPE cType;
    union {
        char c;
        short h;
        int d;
        long int ld;
        unsigned char hhu;
        unsigned short hu;
        unsigned long lu;
        unsigned u;
        float f;
        double lf;
        long double llf;
        unsigned char b;
        long long int lld;
        unsigned long long int llu;
        void* address;
    } data;
} PI_MPI_RTTI;

#endif
