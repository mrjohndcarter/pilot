/*!
********************************************************************************
\file test_dead_wait_reduce.c
\brief Reducing on bundle where one process end is dead.

Scenario:
	A&B -reducer-> Main
A exits
B writes
Main reduces

Result:
1) A exits first: "Process at other end of channel has exited"
2) Main reduces first: "Process exiting leaves earlier operation hung"
*******************************************************************************/

#include <pilot.h>

PI_CHANNEL *channels[2];
PI_BUNDLE *reducer;

static int a_worker(int idx, void *p)
{
    return 0;
}

static int b_worker(int idx, void *p)
{
    PI_Write( channels[1], "%+/d", 222 );
    return 0;
}

int main(int argc, char *argv[])
{
    PI_PROCESS *a;
    PI_PROCESS *b;
    int result;

    PI_Configure(&argc, &argv);

    a = PI_CreateProcess(a_worker, 0, NULL);
    b = PI_CreateProcess(b_worker, 0, NULL);
    channels[0] = PI_CreateChannel(a, PI_MAIN);
    channels[1] = PI_CreateChannel(b, PI_MAIN);

    reducer = PI_CreateBundle(PI_REDUCE, channels, 2);

    PI_StartAll();
    
    PI_Reduce( reducer, "%+/d", &result );

    PI_StopMain(0);
    return 0;
}
