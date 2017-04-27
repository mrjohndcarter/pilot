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
\file pilot.h
\brief Public header file for Pilot
\author John Douglas Carter
\author Bill Gardner
\author Sid Bao

Provides prototypes for all functions used by Pilot application programmers.
*******************************************************************************/

#ifndef PILOT_H
#define PILOT_H

#ifndef PI_NO_OPAQUE
/*!
********************************************************************************
Function prototype for user defined worker functions.

Functions to be assigned to processes must match this prototype.
The work function's return value will be output in the log, but has no
other operational significance.
The two arguments are supplied via PI_CreateProcess.
*******************************************************************************/
typedef int(*PI_WORK_FUNC)(int,void*);

/* Opaque datatypes for function declarations below */
typedef struct OPAQUE PI_PROCESS;
typedef struct OPAQUE PI_CHANNEL;
typedef struct OPAQUE PI_BUNDLE;
#endif

#include "pilot_limits.h"

// define NULL
#include <stddef.h>


/*** Pilot global variables ***/

/*!
********************************************************************************
Specifies whether Pilot will print a banner and configuration info on stdout
when it starts up.
 - 0 = normal print
 - non-0 = quiet
Error messages will print on stderr regardless.
*******************************************************************************/
extern int PI_QuietMode;

/*!
********************************************************************************
Specifies level of error checking to be done by library functions.  The variable
may be set directly by the programmer before calling PI_Configure, or indirectly
by means of command line option -picheck.  If set to N, additional checks up to
and including level N will be done at the expense of performance degradation.

After all Pilot-using code has been exercised, it should not be necessary to
keep using levels 2 and 3 (which have high overhead).

Level 0:
 - Validates many function preconditions (detects user errors)

Level 1: DEFAULT
 - Validates internal tables => PI_INVALID_OBJ error (detects system errors,
   could be user-caused)
 - Checks that variable argument lists match the number of format codes =>
   PI_FORMAT_INVALID error

Level 2:
 - Checks that reader formats match types and lengths of writer formats =>
   PI_FORMAT_MISMATCH error (increases message traffic up to double)

Level 3:
 - Checks that reader pointer arguments (&var) and writer pointer arguments
   (arrays) are likely pointers, not coded as values (var) by mistake, nor NULL
   => PI_BOGUS_POINTER_ARG error  (This is a common mistake in C using
   fscanf-type arguments. The validation algorithm is not guaranteed perfect,
   because data can look like a valid address, and a given OS may layout memory
   in an unexpected fashion. If valid pointers are being rejected, stop using
   this level.)
*******************************************************************************/
extern int PI_CheckLevel;

/*!
********************************************************************************
Specifies whether or not an error detected by a library function should abort the
program.  The variable must be set directly by the programmer, and should only be used
by Pilot developers in order to test error-checking mechanisms.  It is not for use by
Pilot application programmers.  Normal value is 0, causing any error to abort the
program via MPI_Abort.  If set to non-zero, library functions will return after setting
PI_Errno.
*******************************************************************************/
extern int PI_OnErrorReturn;

/*!
********************************************************************************
The last error encountered by the library.

If PI_OnErrorReturn is non-zero and an error was detected by a library
function, then after the function returns, an error code > 0 (from pilate_error.h) will
be here, and further calls to library functions will be undefined.  If no error
occurred, this variable will be zero (=PI_NO_ERROR).
*******************************************************************************/
extern int PI_Errno;

/*! Filename of current library caller. */
extern const char *PI_CallerFile;

/*! Line number of current library caller. */
extern int PI_CallerLine;


/*!
********************************************************************************
\def PI_MAIN
\brief Alias for the 'master' process, whose behavior is found in main().
*******************************************************************************/
#define PI_MAIN 0


/*!
********************************************************************************
\def PP_NARG
\brief Set of macros to count variadic macro arguments
\note By Laurent Deniau, https://groups.google.com/forum/#!topic/comp.std.c/d-6Mj5Lko_s
*******************************************************************************/
#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define PP_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0


/*** functions to call in configuration phase ***/

/*!
********************************************************************************
Establishes a set of processes for Pilot to use.

THIS FUNCTION IS NOT FOR NORMAL USE!  If one is running benchmarks having called
MPI_Init first, and it is desired that Pilot utilize only a subset of existing ranks,
i.e., not all of MPI_COMM_WORLD, then this function can be called prior to PI_Configure
in order to provide the communicator containing the subset.

\param comm The communicator of type MPI_Comm.

\pre PI_Configure() has not been called (without intervening PI_StopMain).
\post PI_configure() will use the supplied communicator in lieu of MPI_COMM_WORLD.
*******************************************************************************/
void PI_SetCommWorld_( void *comm );
#define PI_SetCommWorld( comm ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_SetCommWorld_( comm ))

/*!
********************************************************************************
Initializes the Pilot library.

Must be called before any other Pilot calls, passing argc,argv from main().
This function removes Pilot arguments from argc/argv, and the MPI implementation
may do likewise for its own arguments.  Therefore, PI_Configure() can be called
before the application looks for its own arguments.

Optional command line arguments (all start with "-pi"):

- -picheck=\<level number\>
  - 0-3: see PI_CheckLevel above for explanation of the 4 levels

- -pisvc=\<runtime services\>
  - c: make log of API calls
  - d: perform deadlock detection (uses one additional MPI process)

- -pilog=\<filename\>

\c -picheck overrides any programmer setting of the PI_CheckLevel global variable
made prior to calling \c PI_Configure(). Level N includes all levels below it.

\c -pisvc only causes relevant data to be dumped to the log file. Another program
is needed to analyze and print/visualize the results. Other services are planned
for future versions.

\c -pilog allows the name of the log file to be changed from the default "pilot.log"

\note Only specifying -pilog=fname does not by itself create a log. Some
logging service (presently only "c") must also be selected.

\param argc Number of arguments, argument to main.
\param argv Array of strings, containing arguments.

\return The number of MPI processes available for Pilot process creation. This
number is a total that includes main (which is running and does not need to be
explicitly created) and deadlock detection (if selected).  If N is returned,
then PI_CreateProcess can be called at most N-1 times.

\pre argc/argv are unmodified.
\post MPI and Pilot are initialized. Pilot calls can be made. Pilot
(and possibly MPI) arguments have been removed from argc/argv by shuffling
up the argv array and decrementing argc.

\note Must be called in your program's main().
\note For special purposes like running benchmarks, Pilot can be put into
"bench mode" by calling MPI_Init prior to PI_Configure.
*******************************************************************************/
int PI_Configure_( int *argc, char ***argv );
#define PI_Configure( argc, argv ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Configure_( argc, argv ))

/*!
********************************************************************************
Creates a new process.

Assigns a function pointer as behavior to the new process, and returns a
process pointer. Its default name will be "Pn" where n is its integer process ID.

\param f Pointer to the function this process 'runs'.
\param index An integer used for configuring the work function.
\param opt_pointer A pointer which can be used to supply data to the work function.
\param lang is an internal-use argument to specify C (0) or Fortran (1).

\return A pointer to the process created, or NULL if an error occured.

\pre PI_Configure() has been called.
\post Process has been created, stored in process table.
*******************************************************************************/
PI_PROCESS *PI_CreateProcess_( PI_WORK_FUNC f, int index, void *opt_pointer,
							int lang );
#define PI_CreateProcess( f, index, opt_pointer ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_CreateProcess_( f, index, opt_pointer, 0 ))

/*!
********************************************************************************
Creates a new channel between the specified processes.

Opens a channel between the specified processes, with the specified alias.
Returns a channel pointer.  Its default name will be "Cn:Pf>Pt" where n is its
integer channel ID, and f and t are the from/to process IDs, respectively.
Multiple channels can exist between the same processes.

\param from A pointer to the 'write-end' of the channel.
\param to A pointer to the 'read-end' of the channel.

\return A pointer to the newly created channel, or NULL if an error occured.

\pre None
\post Channel created, entered into channel table.

\note If PI_MAIN/NULL is specified for either process, it represents the
master/main process (rank 0).
*******************************************************************************/
PI_CHANNEL *PI_CreateChannel_( PI_PROCESS *from, PI_PROCESS *to );
#define PI_CreateChannel( from, to ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_CreateChannel_( from, to ))

/*!
********************************************************************************
Specifies which type of bundle to create.
\see PI_CreateBundle
*******************************************************************************/
enum PI_BUNUSE { PI_BROADCAST, PI_SCATTER, PI_GATHER, PI_REDUCE, PI_SELECT };

/*!
********************************************************************************
Creates a channel grouping (bundle) for a particular collective use.

Creates a bundle of channels, such that all the channels must have one
endpoint in common.  Its default name will be "Bn@Pc" where n is its integer
bundle ID, and e is the process ID of the common endpoint.  Returns a bundle
pointer.

\param usage Symbol denoting how the bundle will be used. If PI_Broadcast
 will be called, then code PI_BROADCAST, etc.
\param array Channels to incorporate into bundle.
\param size Number of channels in array.

\return Returns the newly created bundle.

\post All channels in array are marked as being in this bundle.

\note The non-common end of all channels must be unique.
\note A channel can be in only one bundle.
*******************************************************************************/
PI_BUNDLE *PI_CreateBundle_( enum PI_BUNUSE usage, PI_CHANNEL *const array[], int size );
#define PI_CreateBundle( usage, array, size ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_CreateBundle_( usage, array, size ))

/*!
********************************************************************************
Specifies which direction the channels should point after a copy operation.
\see PI_CopyChannels
*******************************************************************************/
enum PI_COPYDIR { PI_SAME, PI_REVERSE };

/*!
********************************************************************************
Copies an array of channels.

Given an array of channel pointers, this function duplicates the channels (i.e.,
for each CHANNEL* in the array, issues PI_CreateChannel on the same pair of
endpoints) and returns their pointers in a same-size array. The order of channel
pointers in the input and output arrays will be the same. If specified, the
endpoints will be reversed (i.e., each channel from P to Q will be copied as a
channel from Q to P). This function makes it convenient to create multiple
channels between, say, a master process and a set of workers, where one set is
for PI_Write usage, one set for PI_Broadcast, and another for PI_Select/Read or
PI_Gather.

\param direction Symbol denoting the direction of the copy. PI_SAME will
preserve the current endpoints; PI_REVERSE will flip the direction.
\param array Channels to be copied.
\param size Number of channels in array.

\return Returns a pointer to the new array of CHANNEL*, same dimension as
the size input.

\post 'size' new channels were created, each having its default name.

\note This function does not copy a bundle. You can copy the array used to
create a bundle, and then create a new bundle -- of same or different usage
-- from the function's output.
\note The main program should call free() on the returned array, since it is
obtained via malloc.
*******************************************************************************/
PI_CHANNEL **PI_CopyChannels_( enum PI_COPYDIR direction, PI_CHANNEL *const array[], int size );
#define PI_CopyChannels( direction, array, size ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_CopyChannels_( direction, array, size ))

/*!
********************************************************************************
Set the friendly name of a process, channel, or bundle.

When created, each object has a default name. If the user wishes to change
them, say for log or error message readability, this function is used.

\param object The PROCESS*, CHANNEL*, or BUNDLE* whose name is to be set.
\param name Friendly name for object. A copy is made of this string up to
PI_MAX_NAMELEN characters. If NULL is supplied, the name is set to "".
*******************************************************************************/
void PI_SetName_( void *object, const char *name );
#define PI_SetName( object, name ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_SetName_( object, name ))

/*!
********************************************************************************
Kicks off parallel processing.

All processes call their assigned functions, and the primary process continues as main.
Other processes DO NOT RETURN from PI_StartAll (unless in "bench mode", see below).

\return MPI rank of this process.  Normally this is of no interest because
only the main process (rank 0) returns.  But if Pilot has been put into
"bench mode" by calling MPI_Init in advance, then all processes will return.
In that case, the user will want to check this value.

\pre PI_Configure must have been called.
\post Parallel execution has begun.

\warning No channels, processes, bundles or selectors may be created once
this has been called.
*******************************************************************************/
int PI_StartAll_(void);
#define PI_StartAll() \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_StartAll_())

/*** functions to call after PI_StartAll ***/

/*!
********************************************************************************
Returns the friendly name of a process, channel, or bundle.

Returns a string containing the friendly name of the given object, or that of
the caller's process if given NULL. This is the name set by the last call to
PI_SetName(), or its default name if none was ever set.

\param object The PROCESS*, CHANNEL*, or BUNDLE* whose name is to be
returned, or NULL to indicate the caller's process.
\return String containing the name of the given object (will not be NULL).
*******************************************************************************/
const char *PI_GetName_( const void *object );
#define PI_GetName( object ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_GetName_( object ))

/*!
********************************************************************************
Writes a number of values to the specified channel.

The format string specifies the types of each variable.  Uses control codes
similar to stdio.h's scanf:
- %d or %i - for integer
- %ld or %li - for long int
- %lld or %lli - for long long int
- %u - for unsigned int
- %lu - for unsigned long
- %llu - for unsigned long long
- %hd or %hi - for short
- %hu - for unsigned short
- %c - for character (printable, may get code-converted)
- %hhu - for unsigned character
- %b - for byte (uninterpreted 8 bits, i.e., unsigned char)
- %f - for float
- %lf - for double
- %Lf - for long double
- %m - for MPI user-defined datatype (next argument is of type MPI_Datatype)
- %s - for C string (NUL-terminated char array)

For %s format on reading, a right-sized char array is allocated and its pointer
is stored in the following argument, e.g., ("%s", &buff) where "char *buff;".

Arrays are specified by inserting the size just before the type, e.g.,
%25d = int[25].  If the size is specified as "*", it is obtained from the
next argument, e.g., ("%*d", 25, intarray).  If the size is specified as "^":
- on writing it is obtained from the next argument as for "*"
- on reading it is obtained from the writer and stored in the next argument,
  then a right-sized array is allocated and its pointer is stored in the
  following argument, e.g., ("%^d", &len, &arrayptr) where "int len, *arrayptr;".

Variable length arrays ("^" flag and "%s" format) are not supported for collective
operations except for PI_Broadcast.

Reduce operations (for PI_Reduce and PI_Write) are specified by inserting the
operator and reduce flag (op/) immediately after %, e.g., %+/d, or %+/25d with
an array.  Supported operaters and acceptable types:
- arithmetic operations, for any integer or floating point type: max, min, +, *
- bit-wise operations, for byte or any integer type: &, |, ^
- logical operations, for any integer type: &&, ||, ^^ (logical xor)
- user-defined operation: mop; the next argument is an object of type MPI_Op (see MPI_Op_create)

\param c Channel to write to.
\param format Format string specifying the type of each variable.

\pre Channel has been created.
\post Data values have been sent to process at read end of channel.

\note %1 (one) is not allowed since it looks too similar to %l (ell).
\note Variables after the format string are passed by value, except for arrays,
following C's normal argument-passing practice.
\note Format strings on read/write ends must match!
*******************************************************************************/
void PI_Write_( PI_CHANNEL *c, const char *format, ... );
#define PI_Write( c, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Write_( c, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Reads a number of values from the specified channel.

The format string specifies the types of each variable (see PI_Write).

\param c Channel to read from.
\param format Format string specifying the type of each variable.

\pre Channel has been created.
\post Variables have been filled with data from process at write end of channel.

\note Variables after the format string must be given by reference (&arg),
not by value, just as with scanf.
\note Format strings on read/write ends must match!
*******************************************************************************/
void PI_Read_( PI_CHANNEL *c, const char *format, ... );
#define PI_Read( c, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Read_( c, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Returns the index of a channel in the bundle that has data to read.

\param b Bundle to select from.
\return Index of Channel to be read.

\pre Bundle has been created.
\post The channel selected is the next to be read from those in the bundle.

\see PI_GetBundleChannel
*******************************************************************************/
int PI_Select_( PI_BUNDLE *b );
#define PI_Select( b ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Select_( b ))

/*!
********************************************************************************
Indicates whether the specified channel can be read.

Can be used to test whether or not a read operation would block.
\param c Channel to test for a queued read.
\retval 1 if Channel has a queued read.
\retval 0 if Channel does not have a queued read.
\pre Channel \p c has been created.
*******************************************************************************/
int PI_ChannelHasData_( PI_CHANNEL *c );
#define PI_ChannelHasData( c ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_ChannelHasData_( c ))

/*!
********************************************************************************
Indicates whether any of the Selector's channels can be read.

Same as PI_Select, except that if none of the Selector's channels is ready
to be read (i.e., PI_ChannelHasData would return 0 for every channel), this
function returns -1.

\param b Selector bundle to test for a queued read.
\return Index of Channel to be read, or -1 if no channel has data.

\pre Selector \p b has been created.
*******************************************************************************/
int PI_TrySelect_( PI_BUNDLE *b );
#define PI_TrySelect( b ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_TrySelect_( b ))

/*!
********************************************************************************
Returns the specified channel from a bundle.

Given an index, returns that channel from the bundle.

\param b Bundle containing the desired channel.
\param index index of the channel to return.
\return Requested channel, or NULL if index was invalid.

\pre Bundle \p b has been created.
*******************************************************************************/
PI_CHANNEL* PI_GetBundleChannel_( const PI_BUNDLE *b, int index );
#define PI_GetBundleChannel( b, index ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_GetBundleChannel_( b, index ))

/*!
********************************************************************************
Returns the size of a bundle.

Provides the number of channels in the specified bundle.

\param b Bundle to return the size for.
\return Number of channels in this bundle.

\pre Bundle \p b has been created.
*******************************************************************************/
int PI_GetBundleSize_( const PI_BUNDLE *b );
#define PI_GetBundleSize( b ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_GetBundleSize_( b ))

/*!
********************************************************************************
Writes to all channels in the specified bundle.

Simultaneously writes the format string and values to every channel
contained in the bundle.

The "^" flag is supported for broadcasting variable length arrays (see
PI_Write).

\param b Broadcaster bundle to write to.
\param format Format string and values to write to the bundle.
\pre Bundle must be a broadcaster bundle.
*******************************************************************************/
void PI_Broadcast_( PI_BUNDLE *b, const char *format, ... );
#define PI_Broadcast( f, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Broadcast_( f, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Write to all channels in the specified bundle.

Simultaneously writes the format string and values to every channel
contained in the bundle. The order in which the channels appeared in the array
that created the bundle determines the order of dispersing data from the sending
arrays.

\param b Scatterer bundle to read from.
\param format Format string and values to write to the bundle.
\pre Bundle must be a scatter bundle.
\pre Each sending location must be an array with sufficient space to hold B values, where B is the bundle size.
*******************************************************************************/
void PI_Scatter_( PI_BUNDLE *b, const char *format, ... );
#define PI_Scatter( f, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Scatter_( f, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Apply a reduce operation to input from all channels in the specified bundle.

Simultaneously reads the format string and values from every channel
contained in the bundle, and collectively applies the specified reduce operation.
The result may be a scalar or an array. The writers must all send the same size data.

See PI_Write for reduce operations.

\param b Reducer bundle to read from.
\param format Format string and values to reduce from the bundle.
\pre Bundle must be a reducer bundle.
*******************************************************************************/
void PI_Reduce_( PI_BUNDLE *b, const char *format, ... );
#define PI_Reduce( f, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Reduce_( f, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Read from all channels in the specified bundle.

Simultaneously reads the format string and values from every channel
contained in the bundle. The order in which the channels appeared in the array
that created the bundle determines the order of storing data in the receiving
arrays.

\param b Gatherer bundle to read from.
\param format Format string and values to read from the bundle.
\pre Bundle must be a gatherer bundle.
\pre Each receiving location must be an array with sufficient space to hold B values, where B is the bundle size.
*******************************************************************************/
void PI_Gather_( PI_BUNDLE *b, const char *format, ... );
#define PI_Gather( f, format, ... ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Gather_( f, format, PP_NARG(__VA_ARGS__), __VA_ARGS__ ))

/*!
********************************************************************************
Starts an internal timer.  Creates a fixed point in time -- the time between
now and another point in time is reported by PI_EndTime().
*******************************************************************************/
void PI_StartTime_( void );
#define PI_StartTime() \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_StartTime_())

/*!
********************************************************************************
Gives the time elapsed since the previous fixed point.

\return The wall-clock time elapsed in seconds sinced PI_StartTime() was called.
*******************************************************************************/
double PI_EndTime_(void);
#define PI_EndTime() \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_EndTime_())

/*!
********************************************************************************
Logs a time-stamped event to the log file.

Allow a user program to make entries in the log file.  If logging to file is
not enabled, the call is a no-op.  The user may wish to check PI_IsLogging()
first.  Each entry will record a time stamp and caller's process number.
*******************************************************************************/
void PI_Log_( const char *text );
#define PI_Log( text ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_Log_( text ))

/*!
********************************************************************************
Returns true if logging to a file is enabled.
*******************************************************************************/
int PI_IsLogging_( void );
#define PI_IsLogging() \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_IsLogging_())

/*!
********************************************************************************
Aborts execution of Pilot application.

Normally called by library functions when error detected, but can be
called by user program that wants to exit abruptly.  Prints message:

<tt>*** PI_Abort *** (MPI process \#N) Pilot process 'name'(arg), \<file\>:\<line\>:
\<errmsg\>\<text\></tt>

where errmsg is derived from errcode if it is in range for Pilot codes, otherwise
"" (e.g., errcode 0 will suppress errmsg).

\param errcode The Pilot error code or 0 (zero).
\param text Extra message to display with the error message.
\param file Should be filled in with \c __FILE__ .
\param line Should be filled in with \c __LINE__ .

\post The entire application is aborted.
*******************************************************************************/
void PI_Abort( const int errcode, const char *text, const char *file, const int line );

/*!
********************************************************************************
Performs clean up for the Pilot library.

Finalizes the underlying MPI library, de-allocates all internal structures.

\param status If logging, value will appear in log, otherwise has no effect.

\pre PI_StartAll was called.
\post No additional Pilot calls may be made after function returns.

\warning Should be called only once, at the end of your program's main().

\note PI_StopMain_ is also called internally by PI_StartAll when each work
function returns, in which case it calls exit().
\note Need to say something about "bench mode."
*******************************************************************************/
void PI_StopMain_( int status );
#define PI_StopMain( status ) \
	(PI_CallerFile = __FILE__, PI_CallerLine = __LINE__ , \
	PI_StopMain_( status ))

#endif
