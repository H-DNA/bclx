#ifndef STACK_H
#define STACK_H

#include "../config.h"			//Configurations

#include "../../memory/memory.h"	//Global Memory Management

#include "stack_blocking.h"		//Blocking Stack using the MCS Lock

#include "stack_blocking2.h"		//Blocking Stack 2 using the MCS Lock

#include "stack_treiber.h"		//Treiber's Stack (testing version)

#include "stack_treiber_test.h"		//Treiber's Stack

#include "stack_eb.h"			//Elimination-Backoff Stack

#include "stack_eb2.h"			//Elimination-Backoff Stack 2

#include "stack_eb_na.h"		//Node-Aware Elimination-Backoff Stack

#include "stack_eb2_na.h"		//Node-Aware Elimination-Backoff Stack 2

#include "stack_eb2_na_test.h"		//Node-Aware Elimination-Backoff Stack 2 (testing version)

#include "stack_ts_stutter.h"		//Time-Stamped Stack using TS-interval&stutter

#include "stack_ts_atomic.h"		//Time-Stamped Stack using TS-interval&atomic

#include "stack_ts_atomic2.h"		//Time-Stamped Stack using TS-atomic

#include "stack_ts_cas.h"		//Time-Stamped Stack using TS-interval&cas

#endif /* STACK_H */
