/*!
********************************************************************************
\file scatter_deadly_embrace.c
\brief Circular wait with two processes reading and one scatter.

Scenario:
	M -main_q-> Q -q_r-> R
	 ----------/\--------^ scatterer
M scatters
Q reads
R reads

Result:
1) any order: "Conflicting channels create deadly embrace"
*******************************************************************************/

#include <pilot.h>

PI_CHANNEL* main_q;
PI_CHANNEL* q_r;

static int Q(int idx, void* ctx)
{
    int recv;
    PI_Read(main_q, "%d", &recv);
    return 0;
}

static int R(int idx, void* ctx)
{
    int recv;
    PI_Read(q_r, "%d", &recv);
    return 0;
}

int main(int argc, char* argv[])
{
    PI_PROCESS* q;
    PI_PROCESS* r;
    PI_CHANNEL* chans[2];
    PI_BUNDLE* scatterer;

    PI_Configure(&argc, &argv);

    q = PI_CreateProcess(Q, 0, NULL);
    r = PI_CreateProcess(R, 0, NULL);
    main_q = PI_CreateChannel(PI_MAIN, q);
    q_r = PI_CreateChannel(q, r);
    chans[0] = PI_CreateChannel(PI_MAIN, q);
    chans[1] = PI_CreateChannel(PI_MAIN, r);
    scatterer = PI_CreateBundle(PI_SCATTER, chans, 2);

    PI_StartAll();

    {
        int data[2] = {88.99}; // One int to 2 channels
        PI_Scatter(scatterer, "%*d", 1, data );
    }

    PI_StopMain(0);
    return 0;
}
