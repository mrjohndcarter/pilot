#ifndef UNITTESTS_H
#define UNITTESTS_H

#include <pilot.h>
#include <pilot_error.h>
#include <CUnit/Basic.h>
#include <stdlib.h>
#include <mpi.h>

typedef CU_ErrorCode (*SuiteRegisterFunc)(void);

/*== Global variables ==*/

extern int default_argc;
extern char** default_argv;
extern int my_rank;

/*== Helper functions ==*/
PI_PROCESS* CreateAliasedProcess(PI_WORK_FUNC f, const char* alias, int index, void* opt_pointer);

/* Use AddTest() instead of CU_add_test() (it works on all nodes). */
void AddTest(CU_pSuite suite, const char* name, CU_TestFunc func);


/*== Suite registration functions ==*/
CU_ErrorCode AddInitSuite(void);
CU_ErrorCode AddSingleRWSuite(void);
CU_ErrorCode AddArrayRWSuite(void);
CU_ErrorCode AddMixedValueSuite(void);
CU_ErrorCode AddSelectorSuite(void);
CU_ErrorCode AddBroadcasterSuite(void);
CU_ErrorCode AddGathererSuite(void);
CU_ErrorCode AddScattererSuite(void);
CU_ErrorCode AddReducerSuite(void);
CU_ErrorCode AddExtraReadWriteSuite(void);
CU_ErrorCode AddFormatSuite(void);
CU_ErrorCode AddConfigSuite(void);


#endif /* UNITTESTS_H */
