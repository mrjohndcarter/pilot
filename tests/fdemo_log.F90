! fdemo_log.F90
! FORTRAN version of demo_log.c used as a confidence/regression test for the Fortran
! API because it calls every function at least once.  Note that when viewing log output
! we currently don't attempt to display message data values (shown as "?").  This is
! because in C, we have the actual data on hand and know the datatypes, but in Fortran
! we just have its address and don't care to do the conversion from the various
! Fortran datatypes.  Everything else in the log viewer will work.
!
! This is a simple demonstration program that exercises all loggable API calls in
! Pilot V3.1.  It uses V3.2 features (support for PI_Scatter and PI_Reduce with
! PI_WriteReduce).
!
! It needs to run with 9 MPI processes and -pisvc=j.
!
! View the .clog2 output using Jumpshot and try right-clicking on various objects.
! You can also use the "spyglass" buttons to sequence through the event info boxes
! in time order.

    MODULE demo_channels
#include "pilot.for"

    ! There are 3 groups of processes
    INTEGER, PARAMETER :: WORKERS1=2, WORKERS2=3, WORKERS3=3
    ! Data array sizes
    INTEGER, PARAMETER :: ARRLEN=5, DATALEN=10000
    
    ! Pilot variables that are used in both PI_MAIN and work functions
    PI_CHANNEL chans(WORKERS1), chans_out(WORKERS1)
    PI_CHANNEL data_c2(WORKERS2), result_c2(WORKERS2)
    PI_CHANNEL data_c3(WORKERS3), result_c3(WORKERS3)
    END MODULE demo_channels
    
    PROGRAM fdemo_log
    USE demo_channels
    ! Pilot variables used in Config Phase and PI_MAIN only
    PI_PROCESS procs1(WORKERS1)
    PI_BUNDLE select_bundle

    PI_PROCESS procs2(WORKERS2)
    PI_BUNDLE gather_bundle, broadcast_bundle

    PI_PROCESS procs3(WORKERS3)
    PI_BUNDLE scatter_bundle, reduce_bundle

    INTEGER(C_INT), EXTERNAL :: WorkFunc1, WorkFunc2, WorkFunc3
    
    INTEGER, TARGET :: i, selected, sum, total, outbuff(DATALEN*WORKERS3)
    CHARACTER, TARGET :: char_a='Y', char_b='N'
    REAL, TARGET :: send(ARRLEN), recv(ARRLEN*WORKERS2)
    PI_CHANNEL, TARGET :: chan
    INTEGER N           ! no. of processes available
    CHARACTER p
    LOGICAL chandata, logging
    DOUBLE PRECISION time
    
    ! Pilot configuration phase return no. of processes available
    CALL PI_Configure( N )
    
    CALL PI_StartTime()

    DO i=1,WORKERS1
        procs1(i) = PI_CreateProcess( WorkFunc1, i )
        CALL PI_SetName( procs1(i), "WorkFunc1" )
        chans(i) = PI_CreateChannel( PI_MAIN, procs1(i) )
        chans_out(i) = PI_CreateChannel( procs1(i), PI_MAIN )
    END DO
    select_bundle = PI_CreateBundle( PI_BUN_SELECT, chans_out, WORKERS1 )
    CALL PI_SetName( select_bundle, "Selector bundle with long name" )

    DO i=1,WORKERS2
        procs2(i) = PI_CreateProcess( WorkFunc2, i )
        data_c2(i) = PI_CreateChannel( PI_MAIN, procs2(i) )
        result_c2(i) = PI_CreateChannel( procs2(i), PI_MAIN )
    END DO
    broadcast_bundle = PI_CreateBundle( PI_BUN_BROADCAST, data_c2, WORKERS2 )
    gather_bundle = PI_CreateBundle( PI_BUN_GATHER, result_c2, WORKERS2 )
    
    DO i=1,WORKERS3
        procs3(i) = PI_CreateProcess( WorkFunc3, i )
        data_c3(i) = PI_CreateChannel( PI_MAIN, procs3(i) )
        result_c3(i) = PI_CreateChannel( procs3(i), PI_MAIN )
    END DO
    CALL PI_CopyChannels( PI_DIR_REVERSE, data_c3, WORKERS3, result_c3 )
    scatter_bundle = PI_CreateBundle( PI_BUN_SCATTER, data_c3, WORKERS3 )
    reduce_bundle = PI_CreateBundle( PI_BUN_REDUCE, result_c3, WORKERS3 )


    ! Start execution phase:  WorkFunc instances spawn off
    ! while PI_MAIN continues below...
    CALL PI_StartAll()
    
    IF ( PI_IsLogging() ) THEN
        p = 'Y'
    ELSE
        p = 'N'
    END IF
    PRINT *, 'Do you have logging turned on? ', p
    
    ! First group of workers:  write out, select/read back
    CALL PI_Write( chans(1), PI_CHARACTER, 1, char_a )      ! 'Y'
    CALL PI_Write( chans(2), PI_CHARACTER, 1, char_b )      ! 'N'

    DO i=1,WORKERS1
        selected = PI_Select( select_bundle )
        selected = PI_TrySelect( select_bundle )
        logging = PI_ChannelHasData( chans_out(i) )
        chan = PI_GetBundleChannel( select_bundle, selected )
        CALL PI_Read( chan, PI_INTEGER, 1, sum )
        PRINT *, 'Process ', selected-1, ' is ready -- The sum is ', sum
    END DO
    PRINT *, 'TrySelect should return -1 now: ', PI_TrySelect( select_bundle )
    
    ! Second group of workers:  broadcast out, gather back
    DO i=1,5
        send(i) = i
    END DO
    CALL PI_Broadcast( broadcast_bundle, PI_REAL, ARRLEN, send )
    CALL PI_Gather( gather_bundle, PI_REAL, ARRLEN, recv )
    
    time = PI_EndTime()
    
    ! Third group of workers:  scatter out, reduce back
    DO i=1,DATALEN*WORKERS3
        outbuff(i) = (i-1) + 1  ! compensate for do loop starts at 1
    END DO
    CALL PI_Scatter( scatter_bundle, PI_INTEGER, DATALEN, outbuff )
    CALL PI_Reduce( reduce_bundle, PI_INTEGER, 1, total, "+" )
    PRINT *
    PRINT *, 'The total was: ', total ! Sum subtotals from workers
       
    time = PI_EndTime()

    CALL PI_StopMain( 0 )   ! All processes synchronize at end
    END PROGRAM fdemo_log


! process functions (hook arg is not used)

    INTEGER(C_INT) FUNCTION WorkFunc1( q, hook )
    USE demo_channels
    INTEGER(C_INT), INTENT(IN) :: q
    TYPE(C_PTR), INTENT(IN) :: hook

    CHARACTER, TARGET :: choice(1), procname*40
    PI_PROCESS, TARGET :: ptr=C_NULL_PTR
    INTEGER, TARGET :: num
    
    CALL PI_Read( chans(q), PI_CHARACTER, 1, choice )

    CALL PI_GetName( ptr, procname )
    PRINT *, 'My name is ', procname
    CALL PI_Log( "Logged by MPE. This application is logged." )
    
    num = 100 + q -1    ! q is 1-based in Fortran, 0-based in C
    CALL PI_Write( chans_out(q), PI_INTEGER, 1, num )   ! 101, 102

    WorkFunc1 = 0
    END FUNCTION WorkFunc1

    INTEGER(C_INT) FUNCTION WorkFunc2( q, hook )
    USE demo_channels
    INTEGER(C_INT), INTENT(IN) :: q
    TYPE(C_PTR), INTENT(IN) :: hook

    INTEGER i
    REAL, TARGET :: array(ARRLEN)

    CALL PI_Read( data_c2(q), PI_REAL, ARRLEN, array )

    DO i=1,ARRLEN
        array(i) = array(i) + 1.1
    END DO

    CALL PI_Write( result_c2(q), PI_REAL, ARRLEN, array )

    WorkFunc2 = 0
    END FUNCTION WorkFunc2

    INTEGER(C_INT) FUNCTION WorkFunc3( q, hook )
    USE demo_channels
    INTEGER(C_INT), INTENT(IN) :: q
    TYPE(C_PTR), INTENT(IN) :: hook

    INTEGER i
    INTEGER, TARGET :: array(DATALEN), sum=0

    ! Receive my portion of scattered array from master
    CALL PI_Read( data_c3(q), PI_INTEGER, DATALEN, array )

    ! Sum it and send back the subtotal (to be reduced +/)
    DO i=1,DATALEN
        sum = sum + array(i)
    END DO

    CALL PI_WriteReduce( result_c3(q), PI_INTEGER, 1, sum, "+" )

    WorkFunc3 = 0
    END FUNCTION WorkFunc3

