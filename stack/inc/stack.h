#ifndef STACK_H
#define STACK_H

#include "../../lib/common.h"	//Common

#include "stack_blocking.h"	//Blocking Stack using the MCS Lock

#include "stack_blocking2.h"	//Blocking Stack 2 using the MCS Lock

#include "stack_treiber.h"	//Treiber Stack

#include "stack_eb.h"		//Elimination-Backoff Stack

#include "stack_ts_stutter.h"	//Time-Stamped Stack using TS-interval&stutter

#include "stack_ts_atomic.h"	//Time-Stamped Stack using TS-interval&atomic

#include "stack_ts_cas.h"	//Time-Stamped Stack using TS-interval&cas

#endif /* STACK_H */
