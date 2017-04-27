! ***************************************************************************
! * Copyright (c) 2008-2017 University of Guelph.
! *                         All rights reserved.
! *
! * This file is part of the Pilot software package.  For license
! * information, see the LICENSE file in the top level directory of the
! * Pilot source distribution.
! ***************************************************************************

! *******************************************************************************
! \file pilot.for
! \brief Public header file for Pilot's Fortran API
! \author Bill Gardner
!
! Provides interfaces for all functions used by Pilot application programmers.
! For collective operations, PI_Broadcast, PI_Gather, and PI_Select are provided,
! but not yet PI_Scatter and PI_Reduce.
!
! Put #include after PROGRAM or MODULE statement (because of the USE below).
!
! CAUTION:  The Pilot calls are actually macros that will have the effect of
! adding text to your line.  If this drives the length beyond 132 columns, you
! may get a "Line truncated" error.  Avoid nested calls and long variable names.
!
! NOTE on channel array indexing:  PI_Select and PI_TrySelect return the index of
! the channel in the array that is ready to read, and they return a Fortran-style
! 1-based index.  PI_TrySelect always returns -1 if no channel is ready to read in
! both C and Fortran contexts.  PI_GetBundleChannel expects a 1-based index in Fortran.
!
! NOTE on Jumpshot log:  You will notice that the event bubbles for PI_Select and
! PI_TrySelect report the C-style 0-based index, because they call MPE from inside
! pilot.c and don't know they were called from Fortran.
!
! [8-Feb-17] Modify macros to handle file/line for PI_Start/EndTime and
!   PI_IsLogging, now that those functions get phase-checked. V3.2 (BG)
! [9-Feb-17] Add support for PI_Scatter and PI_Reduce with new PI_WriteReduce. V3.2 (BG)
! *******************************************************************************

! -------------------------------------------------------------------------
! Module containing "LAYER 2" wrapper functions that relay calls to
! C functions (LAYER 3) which cannot be directly called
! -------------------------------------------------------------------------
    USE fpilot_private

! -------------------------------------------------------------------------
! Pilot global variables that users can set directly (none for now)
! -------------------------------------------------------------------------

! -------------------------------------------------------------------------
! Aliases for Pilot data types in Fortran application program
! -------------------------------------------------------------------------
#define PI_PROCESS TYPE(C_PTR)
#define PI_CHANNEL TYPE(C_PTR)
#define PI_BUNDLE  TYPE(C_PTR)
#define PI_MAIN PI_NullPtrVar

! -------------------------------------------------------------------------
! Usage types for PI_CreateBundle
! -------------------------------------------------------------------------
#define PI_BUN_BROADCAST 0
#define PI_BUN_SCATTER 1
#define PI_BUN_GATHER 2
#define PI_BUN_REDUCE 3
#define PI_BUN_SELECT 4

! -------------------------------------------------------------------------
! Directions for PI_CopyChannels
! -------------------------------------------------------------------------
#define PI_DIR_SAME 0
#define PI_DIR_REVERSE 1

! -------------------------------------------------------------------------
! Data types for sending/receiving on channels and bundles
! Not implemented yet: all complex, all logical, MPI_Datatype
! -------------------------------------------------------------------------
#define PI_CHARACTER    'c'
#define PI_INTEGER      'i'
#define PI_INTEGER1     'i1'
#define PI_INTEGER2     'i2'
#define PI_INTEGER4     'i4'
#define PI_INTEGER8     'i8'
#define PI_REAL         'r'
#define PI_REAL4        'r4'
#define PI_REAL8        'r8'
#define PI_REAL16       'r6'
#define PI_DOUBLE_PRECISION 'dp'

! -------------------------------------------------------------------------
! =LAYER 1=
! Macros to wrap Pilot calls so as to provide file name and line no.
! We suffix file name with C-string's NUL.
! -------------------------------------------------------------------------

! SUBROUTINES

#define PI_Configure(procs) \
        PI_Configure_F((__FILE__//C_NULL_CHAR),__LINE__,procs)
#define PI_CopyChannels(dir,chans,size,copies) \
        PI_CopyChannels_F((__FILE__//C_NULL_CHAR),__LINE__,dir,chans,size,copies)
#define PI_SetName(obj,name) \
        PI_SetName_F((__FILE__//C_NULL_CHAR),__LINE__,obj,name)
#define PI_GetName(obj,name) \
        PI_GetName_F((__FILE__//C_NULL_CHAR),__LINE__,obj,name)
#define PI_StartAll() PI_StartAll_F((__FILE__//C_NULL_CHAR),__LINE__)

#define PI_Read(chan,type,size,buff) \
  PI_Caller_F(Real_PI_Read,(__FILE__//C_NULL_CHAR),__LINE__,chan,type,size,C_LOC(buff))
#define PI_Write(chan,type,size,buff) \
  PI_Caller_F(Real_PI_Write,(__FILE__//C_NULL_CHAR),__LINE__,chan,type,size,C_LOC(buff))
#define PI_WriteReduce(chan,type,size,buff,oper) \
  PI_Caller_F(Real_PI_Write,(__FILE__//C_NULL_CHAR),__LINE__,chan,type,size,C_LOC(buff),oper)
#define PI_Broadcast(bund,type,size,buff) \
  PI_Caller_F(Real_PI_Broadcast,(__FILE__//C_NULL_CHAR),__LINE__,bund,type,size,C_LOC(buff))
#define PI_Gather(bund,type,size,buff) \
  PI_Caller_F(Real_PI_Gather,(__FILE__//C_NULL_CHAR),__LINE__,bund,type,size,C_LOC(buff))
#define PI_Scatter(bund,type,size,buff) \
  PI_Caller_F(Real_PI_Scatter,(__FILE__//C_NULL_CHAR),__LINE__,bund,type,size,C_LOC(buff))
#define PI_Reduce(bund,type,size,buff,oper) \
  PI_Caller_F(Real_PI_Reduce,(__FILE__//C_NULL_CHAR),__LINE__,bund,type,size,C_LOC(buff),oper)
#define PI_Log(text) PI_Log_F((__FILE__//C_NULL_CHAR),__LINE__,text)
#define PI_StartTime() PI_StartTime_F((__FILE__//C_NULL_CHAR),__LINE__)
#define PI_StopMain(status) PI_StopMain_F((__FILE__//C_NULL_CHAR),__LINE__,status)

! FUNCTIONS

#define PI_CreateProcess(workfunc,index) \
        PI_CreateProcess_F((__FILE__//C_NULL_CHAR),__LINE__,workfunc,index)
#define PI_CreateChannel(from,to) \
        PI_CreateChannel_F((__FILE__//C_NULL_CHAR),__LINE__,from,to)
#define PI_CreateBundle(usage,array,size) \
        PI_CreateBundle_F((__FILE__//C_NULL_CHAR),__LINE__,usage,array,size)
#define PI_ChannelHasData(chan) \
        PI_ChannelHasData_F((__FILE__//C_NULL_CHAR),__LINE__,chan)
#define PI_Select(bund) \
        PI_Select_F((__FILE__//C_NULL_CHAR),__LINE__,bund)
#define PI_TrySelect(bund) \
        PI_TrySelect_F((__FILE__//C_NULL_CHAR),__LINE__,bund)
#define PI_GetBundleChannel(bund,index) \
        PI_GetBundleChannel_F((__FILE__//C_NULL_CHAR),__LINE__,bund,index)
#define PI_GetBundleSize(bund) \
        PI_GetBundleSize_F((__FILE__//C_NULL_CHAR),__LINE__,bund)
#define PI_IsLogging() PI_IsLogging_F((__FILE__//C_NULL_CHAR),__LINE__)
#define PI_EndTime() PI_EndTime_F((__FILE__//C_NULL_CHAR),__LINE__)
#define PI_Abort(text) PI_Abort_F((__FILE__//C_NULL_CHAR),__LINE__,text)
