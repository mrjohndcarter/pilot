/* demo_log.c

This is a simple demonstration program that exercises all loggable API calls in Pilot V3.1.
It also includes calls that are not loggable so it can be used as a kind of confidence test.

It needs to run with 9 MPI processes and -pisvc=j.

View the .clog2 output using Jumpshot and try right-clicking on various objects.
You can also use the "spyglass" buttons to sequence through the event info boxes
in time order.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pilot.h>

// There are 3 groups of processes
#define WORKERS1 2      // for selecting
#define WORKERS2 3      // for broadcast/gather
#define WORKERS3 3      // for scatter/reduce

// Pilot variables
PI_PROCESS *procs1[WORKERS1];
PI_CHANNEL *chans[WORKERS1], *chans_out[WORKERS1];
PI_BUNDLE *select_bundle;

PI_PROCESS *procs2[WORKERS2];
PI_CHANNEL *data_c2[WORKERS2], *result_c2[WORKERS2]; 
PI_BUNDLE *broadcast_bundle, *gather_bundle;

PI_PROCESS *procs3[WORKERS3];
PI_CHANNEL *data_c3[WORKERS3], **result_c3;
PI_BUNDLE *scatter_bundle, *reduce_bundle;

// Runs on each worker process
int WorkFunc1( int q, void *p ) {
    int i; 
    char choice[1];
    
    PI_Read( chans[q], "%c", choice );

    printf( "My name is %s\n", PI_GetName( NULL ) );    // WorkFunc1
    PI_Log( "Logged by MPE. This application is logged." );
    
    PI_Write( chans_out[q], "%d", 100+q );	// 100, 101

    return 0;
}

#define ARRLEN 5
int WorkFunc2( int q, void *p ) {
    int i;
    float array[ARRLEN];

    PI_Read( data_c2[q], "%5f", array );

    for ( i = 0; i < ARRLEN; i++ ) {
        array[i] = array[i] + 1.1;
    }

    PI_Write( result_c2[q], "%*f", ARRLEN, array );
    return 0;
}

#define DATALEN 10000
int WorkFunc3( int index, void *dummy ) {
    int i, array[DATALEN], sum=0;
    
    // Receive my portion of scattered array from master
    PI_Read( data_c3[index], "%*d", DATALEN, array );

    // Sum it and send back the subtotal (to be reduced +/)
    for ( i = 0; i < DATALEN; i++ ) sum += array[i];
    PI_Write( result_c3[index], "%+/d", sum );
    return 0;
}


int main( int argc, char *argv[] ) {
    int i, sum, outbuff[DATALEN*WORKERS3], total;
    char char_a = 'Y', char_b = 'N';
    float send[ARRLEN], recv[ARRLEN*WORKERS2];

    PI_Configure( &argc, &argv );   // Start configuration phase
    
    PI_StartTime();
       
    for ( i = 0; i < WORKERS1; i++ ) {
        procs1[i] = PI_CreateProcess( WorkFunc1, i, NULL );
	PI_SetName( procs1[i], "WorkFunc1" );
        chans[i] = PI_CreateChannel( PI_MAIN, procs1[i] );
        chans_out[i] = PI_CreateChannel( procs1[i], PI_MAIN );
    }
    select_bundle = PI_CreateBundle( PI_SELECT, chans_out, WORKERS1 );
    PI_SetName( select_bundle, "Selector bundle with long name" );
    
    for ( i = 0; i < WORKERS2; i++ ) {
        procs2[i] = PI_CreateProcess( WorkFunc2, i, NULL );
        data_c2[i] = PI_CreateChannel( PI_MAIN, procs2[i] );
        result_c2[i] = PI_CreateChannel( procs2[i], PI_MAIN );
    }
    broadcast_bundle = PI_CreateBundle( PI_BROADCAST, data_c2, WORKERS2 );
    gather_bundle = PI_CreateBundle( PI_GATHER, result_c2, WORKERS2 );

    for ( i = 0; i < WORKERS3; i++ ) {
        procs3[i] = PI_CreateProcess( WorkFunc3, i, NULL );
        data_c3[i] = PI_CreateChannel( PI_MAIN, procs3[i] );
    }
    result_c3 = PI_CopyChannels( PI_REVERSE, data_c3, WORKERS3 );
    scatter_bundle = PI_CreateBundle( PI_SCATTER, data_c3, WORKERS3 );
    reduce_bundle = PI_CreateBundle( PI_REDUCE, result_c3, WORKERS3 );
    

    // Start execution phase:  WorkFunc instances spawn off
    // while PI_MAIN continues below...
    PI_StartAll();

    printf( "Do you have logging turned on? %c\n", PI_IsLogging() ? char_a : char_b ); // Y

    // First group of workers:  write out, select/read back
    PI_Write( chans[0], "%c", char_a );		// 'Y'
    PI_Write( chans[1], "%c", char_b );		// 'N'

    for ( i = 0; i < PI_GetBundleSize( select_bundle ); i++ ) {
        int selected = PI_Select( select_bundle );
        PI_TrySelect( select_bundle );
        PI_ChannelHasData(chans_out[i]);
        PI_Read( PI_GetBundleChannel( select_bundle, selected ), "%d", &sum );
        printf( "Process %d is ready -- The sum is %d.\n", selected, sum );
    }
    printf( "TrySelect should return -1 now: %d\n", PI_TrySelect( select_bundle ) );
    
    // Second group of workers:  broadcast out, gather back
    for ( i = 0; i < ARRLEN; i++ ) send[i] = i + 1;
    PI_Broadcast( broadcast_bundle, "%*f", ARRLEN, send );
    PI_Gather( gather_bundle, "%*f", ARRLEN, recv );
    
    PI_EndTime();
    
    // Third group of workers:  scatter out, reduce back
    for ( i = 0; i < DATALEN*WORKERS3; i++ ) outbuff[i] = i + 1;
    PI_Scatter( scatter_bundle, "%*d", DATALEN, outbuff );
    
    // Sum subtotals from workers
    PI_Reduce( reduce_bundle, "%+/d", &total );
    printf( "\nThe total was: %d\n", total );
    
    PI_EndTime();

    PI_StopMain( 0 );   // All processes synchronize at end
    free( result_c3 );
    return 0;
}
