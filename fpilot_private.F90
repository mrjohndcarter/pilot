! ***************************************************************************
! * Copyright (c) 2008-2017 University of Guelph.
! *                         All rights reserved.
! *
! * This file is part of the Pilot software package.  For license
! * information, see the LICENSE file in the top level directory of the
! * Pilot source distribution.
! ***************************************************************************

! *******************************************************************************
! \file fpilot_private.f90
! \brief Fortran wrapper functions needed to call Pilot's C API
! \author Bill Gardner
!
! These functions form LAYER 2 of the Fortran API, which receive calls from
! LAYER 1 macros in pilot.for, do some argument massaging, and call the real
! C API functions (LAYER 3).
!
! NOTE: Probably a fair bit of common massaging can be factored out, or
! function/subr pointers could be used to reduce redundant code.
!
! Checked out with these compilers:
!   Intel Fortran Version 11.0, 11.1, 12.1.3
!   Sun Fortran 95 8.4
!   gcc Fortran 5.3.0
!
! [16-Jan-10] Substitute fixed-len buffer for allocatable (variable length)
!   CHARACTER scalar (Fortran 2003 feature) for Intel 11.0.
!   Make usage of C_F_POINTER compatible with Sun 8.4. V1.2a (BG)
! [1-Oct-13] Change arg list checking from sentinels to no. args. V2.1 (BG)
! [16-May-14] Changed C_LOC(file) to C_LOC(file(1:1)) to satisfy gfortran,
!   similar for other C-string arg refs. V3.0 (BG)
! [8-Feb-17] Provide _F/Real wrappers to handle file/line for PI_Start/EndTime
!   and PI_IsLogging, now that those functions get phase-checked. V3.2 (BG)
! [9-Feb-17] PI_TrySelect was incrementing a -1 return to 0. V3.2 (BG)
! [9-Feb-17] Add support for PI_Scatter and PI_Reduce with PI_WriteReduce. V3.2 (BG)
! *******************************************************************************

! Needed for PI_MAX_NAMELEN; rest won't do any harm
#include "pilot_limits.h"

MODULE fpilot_private

! -------------------------------------------------------------------------
! Pilot global variables
!
! Introduce PI_NullPtrVar to workaround problem with iso_c_binding.c_null_ptr_
! undefined external in Sun compiler. Use in place of C_NULL_PTR.
! -------------------------------------------------------------------------
USE, INTRINSIC :: iso_c_binding
TYPE(C_PTR), BIND(C, NAME='PI_CallerFile') :: PI_CallerFile ! const char*
INTEGER(C_INT), BIND(C, NAME='PI_CallerLine') :: PI_CallerLine
TYPE(C_PTR) PI_NullPtrVar   ! initialized by PI_Configure_

! -------------------------------------------------------------------------
! =TO LAYER 3=
! Procedure interfaces redirecting calls to pilot.h C API
!
! Since this is Fortran calling C, we specify VALUE for all args.
! -------------------------------------------------------------------------
INTERFACE
    INTEGER(C_INT) FUNCTION Real_PI_Configure( pargc, pargv ) &
                BIND(C, NAME='PI_Configure_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: pargc ! int*
    TYPE(C_PTR), VALUE :: pargv ! char***
    END FUNCTION

    TYPE(C_PTR) FUNCTION Real_PI_CreateProcess( workfunc, index, hook, lang ) &
                BIND(C, NAME='PI_CreateProcess_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_FUNPTR), VALUE :: workfunc ! int(*)(int*,void**)
    INTEGER(C_INT), VALUE :: index  ! int
    TYPE(C_PTR), VALUE :: hook  ! void*
    INTEGER(C_INT), VALUE :: lang   ! int
    END FUNCTION

    TYPE(C_PTR) FUNCTION Real_PI_CreateChannel(from,to) &
                BIND(C, NAME='PI_CreateChannel_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: from, to  ! PI_PROCESS*
    END FUNCTION

    TYPE(C_PTR) FUNCTION Real_PI_CopyChannels(dir,array,size) &
                BIND(C, NAME='PI_CopyChannels_')
    USE, INTRINSIC :: iso_c_binding
    INTEGER(C_INT), VALUE :: dir, size  ! enum, int
    TYPE(C_PTR), VALUE :: array ! PI_CHANNEL**
    END FUNCTION

    TYPE(C_PTR) FUNCTION Real_PI_CreateBundle(usage,array,size) &
                BIND(C, NAME='PI_CreateBundle_')
    USE, INTRINSIC :: iso_c_binding
    INTEGER(C_INT), VALUE :: usage  ! enum PI_BUNUSE
    TYPE(C_PTR), VALUE :: array ! PI_CHANNEL**
    INTEGER(C_INT), VALUE :: size   ! int
    END FUNCTION

    SUBROUTINE Real_PI_SetName(obj,name) BIND(C, NAME='PI_SetName_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: obj, name ! PI_PROCESS/CHANNEL/BUNDLE*, char*
    END SUBROUTINE

    TYPE(C_PTR) FUNCTION Real_PI_GetName(obj) BIND(C, NAME='PI_GetName_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: obj   ! PI_PROCESS/CHANNEL/BUNDLE*
    END FUNCTION

    INTEGER(C_INT) FUNCTION Real_PI_StartAll() BIND(C, NAME='PI_StartAll_')
    USE, INTRINSIC :: iso_c_binding
    END FUNCTION

    SUBROUTINE Real_PI_Read(chan,format,nargs,size,array) &
                BIND(C, NAME='PI_Read_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: chan  ! PI_CHANNEL*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    SUBROUTINE Real_PI_Write(chan,format,nargs,size,array) &
                BIND(C, NAME='PI_Write_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: chan  ! PI_CHANNEL*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    SUBROUTINE Real_PI_Broadcast(bund,format,nargs,size,array) &
                BIND(C, NAME='PI_Broadcast_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    SUBROUTINE Real_PI_Reduce(bund,format,nargs,size,array) &
                BIND(C, NAME='PI_Reduce_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    SUBROUTINE Real_PI_Gather(bund,format,nargs,size,array) &
                BIND(C, NAME='PI_Gather_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    SUBROUTINE Real_PI_Scatter(bund,format,nargs,size,array) &
                BIND(C, NAME='PI_Scatter_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    TYPE(C_PTR), VALUE :: format    ! char*
    INTEGER(C_INT), VALUE :: nargs, size    ! int
    TYPE(C_PTR), VALUE :: array ! void*
    END SUBROUTINE

    INTEGER(C_INT) FUNCTION Real_PI_ChannelHasData(chan) &
                BIND(C, NAME='PI_ChannelHasData_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: chan  ! PI_CHANNEL*
    END FUNCTION

    INTEGER(C_INT) FUNCTION Real_PI_Select(bund) BIND(C, NAME='PI_Select_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    END FUNCTION

    INTEGER(C_INT) FUNCTION Real_PI_TrySelect(bund) BIND(C, NAME='PI_TrySelect_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    END FUNCTION

    TYPE(C_PTR) FUNCTION Real_PI_GetBundleChannel(bund,index) &
                BIND(C, NAME='PI_GetBundleChannel_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    INTEGER(C_INT), VALUE :: index  ! int
    END FUNCTION

    INTEGER(C_INT) FUNCTION Real_PI_GetBundleSize(bund) &
                BIND(C, NAME='PI_GetBundleSize_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: bund  ! PI_BUNDLE*
    END FUNCTION

    SUBROUTINE Real_PI_StartTime() BIND(C, NAME='PI_StartTime_')
    USE, INTRINSIC :: iso_c_binding
    END SUBROUTINE

    REAL(C_DOUBLE) FUNCTION Real_PI_EndTime() BIND(C, NAME='PI_EndTime_')
    USE, INTRINSIC :: iso_c_binding
    END FUNCTION

    INTEGER(C_INT) FUNCTION Real_PI_IsLogging() BIND(C, NAME='PI_IsLogging_')
    USE, INTRINSIC :: iso_c_binding
    END FUNCTION

    SUBROUTINE Real_PI_Log(text) BIND(C, NAME='PI_Log_')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: text  ! const char*
    END SUBROUTINE

    SUBROUTINE Real_PI_Abort(errcode,text,file,line) BIND(C, NAME='PI_Abort')
    USE, INTRINSIC :: iso_c_binding
    INTEGER(C_INT), VALUE :: errcode, line  ! int
    TYPE(C_PTR), VALUE :: text, file    ! const char*
    END SUBROUTINE

    SUBROUTINE Real_PI_StopMain(status) BIND(C, NAME='PI_StopMain_')
    USE, INTRINSIC :: iso_c_binding
    INTEGER(C_INT), VALUE :: status ! int
    END SUBROUTINE

    SUBROUTINE Real_free(loc)  BIND(C, NAME='free')
    USE, INTRINSIC :: iso_c_binding
    TYPE(C_PTR), VALUE :: loc   ! void*
    END SUBROUTINE
END INTERFACE

! -------------------------------------------------------------------------
! =LAYER 2=
! Fortran replacement functions for those that can't be called directly
! (which is now all of them since the last few became phase-checked in V3.1)
!
! For these args, we *don't* specify VALUE because it's Fortran calling Fortran.
! -------------------------------------------------------------------------
CONTAINS
    ! ---------------------------------------------------------------------
    ! PI_Configure: obtain the command line args and convert to C-strings
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_Configure_F(file,line,procs)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    INTEGER, INTENT(OUT) :: procs

    INTEGER, TARGET :: argc
    TYPE(C_PTR), TARGET :: argv
    CHARACTER*80, DIMENSION(:), ALLOCATABLE, TARGET :: arg
    TYPE(C_PTR), DIMENSION(:), ALLOCATABLE, TARGET :: argptr

    PI_NullPtrVar = C_NULL_PTR  ! initialize global var

    ! Since Fortran main program does not get called with argc/argv,
    ! we will fetch them here using IARGC & GETARG.
    ! IARGC returns no. args beyond name; the Unix argc convention
    ! includes the name, so increment the count.
    argc = IARGC() + 1

    ALLOCATE( arg(argc) )       ! allocate and fill arg array
    ALLOCATE( argptr(argc) )    ! fill parallel array with char* ptrs
    argv = C_LOC(argptr)
    DO i = 1,argc
        CALL GETARG(i-1,arg(i)) ! 0 gives prog name
        n = min(LEN(arg), 1+LEN_TRIM(arg(i)))   ! position for NUL
        arg(i)(n:n) = C_NULL_CHAR
        argptr(i) = C_LOC(arg(i))
    END DO

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

     ! report no. of MPI processes
    procs = Real_PI_Configure( C_LOC(argc), C_LOC(argv) )

    DEALLOCATE( arg )
    DEALLOCATE( argptr )
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_GetName
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_GetName_F(file,line,obj,name)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), TARGET, INTENT(IN) :: obj  ! PI_PROCESS/CHANNEL/BUNDLE*
    CHARACTER*(*), INTENT(OUT) :: name

    ! Fortran pointer to a character array, size TBD
    CHARACTER(C_CHAR), POINTER :: cname(:)
    TYPE(C_PTR) cloc        ! const char *PI_GetName()
    LOGICAL foundNul

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

    ! make cname -> returned char* string
    ! for size, use max possible since we'll look for NUL below
    cloc = Real_PI_GetName(obj)
    CALL C_F_POINTER( cloc, cname, [PI_MAX_NAMELEN] )

    ! find NUL terminator and copy string to output arg
    foundNul = .FALSE.
    DO i=1,LEN(name)
        IF ( cname(i).EQ.C_NULL_CHAR ) foundNul = .TRUE.
        IF ( foundNul ) THEN
            name(i:i) = ' '
        ELSE
            name(i:i) = cname(i)
        END IF
    END DO
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_CreateProcess: specify Fortran-style call for workfunc
    ! ---------------------------------------------------------------------
    TYPE(C_PTR) FUNCTION PI_CreateProcess_F(file,line,workfunc,index)
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    INTERFACE
        INTEGER(C_INT) FUNCTION workfunc(idx,hook) BIND(C)
        USE, INTRINSIC :: iso_c_binding
        INTEGER(C_INT) idx
        TYPE(C_PTR) hook
        END FUNCTION
    END INTERFACE
    INTEGER, INTENT(IN) :: index

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

    ! Since no void* in Fortran, just send NULL for hook arg
    ! Arg 1 => Fortran-style call by ref workfunc
    PI_CreateProcess_F = &
        Real_PI_CreateProcess(C_FUNLOC(workfunc),index,PI_NullPtrVar,1)
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_CreateChannel
    ! ---------------------------------------------------------------------
    TYPE(C_PTR) FUNCTION PI_CreateChannel_F(file,line,from,to)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), INTENT(IN) :: from, to ! PI_PROCESS*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_CreateChannel_F = Real_PI_CreateChannel(from,to)
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_CreateBundle
    ! ---------------------------------------------------------------------
    TYPE(C_PTR) FUNCTION PI_CreateBundle_F(file,line,usage,array,size)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line, usage, size
    TYPE(C_PTR), TARGET, INTENT(IN) :: array(*) ! PI_CHANNEL*[]

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_CreateBundle_F = Real_PI_CreateBundle(usage,C_LOC(array),size)
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_CopyChannels
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_CopyChannels_F(file,line,dir,chans,size,copies)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line, dir, size
    TYPE(C_PTR), TARGET, INTENT(IN) :: chans(*) ! PI_CHANNEL*[]
    TYPE(C_PTR), INTENT(OUT) :: copies(*)   ! PI_CHANNEL*[]

    ! Fortran pointer to a C_PTR array, size TBD
    TYPE(C_PTR), POINTER :: copychan(:) ! PI_CHANNEL*[]
    TYPE(C_PTR) :: copyptr      ! PI_CHANNEL **PI_CopyChannels()

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

    ! returns pointer to malloc'd array
    copyptr = Real_PI_CopyChannels(dir,C_LOC(chans),size)

    ! make copychan -> returned array of PI_CHANNEL*
    CALL C_F_POINTER( copyptr, copychan, (/size/) )
    DO i = 1,size           ! copy PI_CHANNEL*'s to output array
        copies(i) = copychan(i)
    END DO
    CALL Real_free( copyptr )   ! free the malloc'd array
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_SetName
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_SetName_F(file,line,obj,name)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file, name
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), TARGET, INTENT(IN) :: obj  ! PI_PROCESS/CHANNEL/BUNDLE*

    CHARACTER*(PI_MAX_NAMELEN), TARGET :: cname

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

    ! copy Fortran name into C name buffer and add NUL termination
    n = min(LEN(cname), 1+LEN_TRIM(name))   ! position for NUL
    cname(1:n) = name(1:n)
    cname(n:n) = C_NULL_CHAR

    CALL Real_PI_SetName(obj,C_LOC(cname(1:1)))
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_StartAll: doesn't return function value
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_StartAll_F(file,line)
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    N = Real_PI_StartAll()      ! throw away return value (rank)
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_Caller
    !
    ! This wrapper handles calls to PI_Read, PI_Write, PI_Broadcast, PI_Gather
    ! PI_Scatter, PI_Reduce, and PI_WriteReduce
    !
    ! The oper argument is only used by the last 3
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_Caller_F(subr,file,line,cb,type,size,buff,oper)
    INTERFACE
        SUBROUTINE subr(cb,format,nargs,size,array) BIND(C)
        USE, INTRINSIC :: iso_c_binding
        TYPE(C_PTR), VALUE :: cb    ! PI_CHANNEL* or PI_BUNDLE*
        TYPE(C_PTR), VALUE :: format    ! char*
        INTEGER(C_INT), VALUE :: nargs, size    ! int
        TYPE(C_PTR), VALUE :: array ! void*
        END SUBROUTINE
    END INTERFACE
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line, size
    TYPE(C_PTR), INTENT(IN) :: cb, buff ! PI_CHANNEL* or PI_BUNDLE*, void*
    CHARACTER*(*), INTENT(IN) :: type
    CHARACTER*(*), INTENT(IN), OPTIONAL :: oper

    CHARACTER*8, TARGET :: format

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    IF ( PRESENT(oper) ) THEN   ! make format string with reduce oper if supplied
        format = "%" // oper // "/*F" // type // C_NULL_CHAR
    ELSE
        format = "%*F" // type // C_NULL_CHAR
    END IF
    CALL subr(cb,C_LOC(format(1:1)),2,size,buff)
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_ChannelHasData
    ! ---------------------------------------------------------------------
    LOGICAL FUNCTION PI_ChannelHasData_F(file,line,chan)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), INTENT(IN) :: chan ! PI_CHANNEL*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_ChannelHasData_F = Real_PI_ChannelHasData(chan).NE.0
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_Select
    ! ---------------------------------------------------------------------
    INTEGER FUNCTION PI_Select_F(file,line,bund)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), INTENT(IN) :: bund ! PI_BUNDLE*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_Select_F = Real_PI_Select(bund) + 1  ! add 1 to C 0-based index
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_TrySelect
    ! ---------------------------------------------------------------------
    INTEGER FUNCTION PI_TrySelect_F(file,line,bund)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), INTENT(IN) :: bund ! PI_BUNDLE*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_TrySelect_F = Real_PI_TrySelect(bund)    ! add 1 to C 0-based index (unless -1)
    IF ( PI_TrySelect_F .GE. 0 ) PI_TrySelect_F = PI_TrySelect_F + 1
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_GetBundleChannel
    ! ---------------------------------------------------------------------
    TYPE(C_PTR) FUNCTION PI_GetBundleChannel_F(file,line,bund,index)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line, index
    TYPE(C_PTR), INTENT(IN) :: bund ! PI_BUNDLE*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line

    ! deduct 1 to give C 0-based index
    PI_GetBundleChannel_F = Real_PI_GetBundleChannel(bund,index-1)
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_GetBundleSize
    ! ---------------------------------------------------------------------
    INTEGER FUNCTION PI_GetBundleSize_F(file,line,bund)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    TYPE(C_PTR), INTENT(IN) :: bund ! PI_BUNDLE*

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_GetBundleSize_F = Real_PI_GetBundleSize(bund)
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_StartTime
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_StartTime_F(file,line)
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    CALL Real_PI_StartTime()
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_EndTime
    ! ---------------------------------------------------------------------
    DOUBLE PRECISION FUNCTION PI_EndTime_F(file,line)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    
    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_EndTime_F = Real_PI_EndTime()
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_IsLogging
    ! ---------------------------------------------------------------------
    LOGICAL FUNCTION PI_IsLogging_F(file,line)
    USE, INTRINSIC :: iso_c_binding
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    PI_IsLogging_F = Real_PI_IsLogging().NE.0
    END FUNCTION

    ! ---------------------------------------------------------------------
    ! PI_Log
    !   Truncates text after 255 characters
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_Log_F(file,line,text)
    CHARACTER*(*), TARGET, INTENT(IN) :: file, text
    INTEGER, INTENT(IN) :: line

!   CHARACTER(LEN=:), ALLOCATABLE, TARGET :: buff   ! F2003
    CHARACTER(LEN=256), TARGET :: buff
    INTEGER tlen

    ! allocate buffer for copy of text with NUL termination
    tlen = LEN_TRIM(text)
    tlen = MIN(tlen,LEN(text)-1)    ! leave room for NUL
!   ALLOCATE( CHARACTER*(tlen+1) :: buff )

    ! copy Fortran text into C buffer and add NUL termination
    buff = text(1:tlen) // C_NULL_CHAR

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    CALL Real_PI_Log( C_LOC(buff(1:1)) )
!   DEALLOCATE( buff )
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_Abort
    !   Truncates text after 255 characters
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_Abort_F(file,line,text)
    CHARACTER*(*), TARGET, INTENT(IN) :: file, text
    INTEGER, INTENT(IN) :: line

!   CHARACTER(LEN=:), ALLOCATABLE, TARGET :: buff   ! F2003
    CHARACTER(LEN=256), TARGET :: buff
    INTEGER tlen

    ! allocate buffer for copy of text with NUL termination
    tlen = LEN_TRIM(text)
    tlen = MIN(tlen,LEN(text)-1)    ! leave room for NUL
!   ALLOCATE( CHARACTER*(tlen+1) :: buff )

    ! copy Fortran text into C buffer and add NUL termination
    buff = text(1:tlen) // C_NULL_CHAR

    ! 0 = user abort; send file/line from macro
    CALL Real_PI_Abort( 0, C_LOC(buff(1:1)), C_LOC(file(1:1)), line )

    ! no need to deallocate since not returning
    END SUBROUTINE

    ! ---------------------------------------------------------------------
    ! PI_StopMain
    ! ---------------------------------------------------------------------
    SUBROUTINE PI_StopMain_F(file,line,status)
    CHARACTER*(*), TARGET, INTENT(IN) :: file
    INTEGER, INTENT(IN) :: line
    INTEGER, INTENT(IN) :: status

    PI_CallerFile = C_LOC(file(1:1))    ! addr of string
    PI_CallerLine = line
    CALL Real_PI_StopMain( status )
    END SUBROUTINE
END MODULE fpilot_private
