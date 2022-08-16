#ifndef MEMORY_H
#define MEMORY_H

#include "../config.h"		// Configurations

//#include "memory_lb.h"		// Using It with Lock-Based Data Structures Only

#include "memory_nmr.h"		// Using No Memory Reclamation

#include "memory_hp.h"		// Using Hazard Pointers [Michael, PODC'02 & TPDC'04]

//#include "memory_ebr.h"		// Using Epoch-Based Reclamation [Fraser, PhD'04] & [Hart et al., IPDPS'06 & JPDC'07]

//#include "memory_ebr2.h"	// Using Epoch-Based Reclamation [Wen et al., PPoPP'18]

//#include "memory_ebr3.h"	// Using Epoch-Based Reclamation [Herlihy et al., Book'20]

//#include "memory_he.h"		// Using Hazard Eras [Ramalhete & Correia, SPAA'17]

//#include "memory_ibr.h"		// Using Interval-Based Reclamation (2GEIBR) [Wen et al., PPoPP'18]

#include "memory_dang.h"	// Using Hazard Pointers + the TAKEN Field

#include "memory_dang2.h"	// Using Hazard Pointers + SPSC Bounded Hosted Queues (push_rget)

#include "memory_dang3.h"	// Using Hazard Pointers + SPSC Bounded Hosted Queues (push_load)

#include "memory_dang4.h"	// Using Hazard Pointers + SPSC Unbounded Hosted Pools (push_rget)

#include "memory_bl.h"		// Baseline: Using Hazard Pointers - Scanning

#include "memory_bl2.h"		// Baseline 2: Using Hazard Pointers + Minimum Locality

#include "memory_bl3.h"		// Baseline 3: Using Hazard Pointers + Maximum Locality

#endif /* MEMORY_H */
