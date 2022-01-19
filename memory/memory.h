#ifndef MEMORY_H
#define MEMORY_H

#include "../config.h"		// Configurations

#include "memory_hp.h"		// Using Hazard Pointers [Michael, PODC'02 & TPDC'04]

#include "memory_ebr.h"		// Using Epoch-Based Reclamation [Fraser, PhD'04] & [Hart et al., IPDPS'06 & JPDC'07]

#include "memory_ebr2.h"	// Using Epoch-Based Reclamation [Wen et al., PPoPP'18]

#include "memory_he.h"		// Using Hazard Eras [Ramalhete & Correia, SPAA'17]

#include "memory_ibr.h"		// Using Interval-Based Reclamation (2GEIBR) [Wen et al., PPoPP'18]

#include "memory_dang.h"	// Using Data Locality

#include "memory_dang2.h"	// Using with Lock-Based Data Structures Only

#include "memory_dang3.h"	// Using no Reclamation

#endif /* MEMORY_H */
