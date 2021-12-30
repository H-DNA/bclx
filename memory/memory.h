#ifndef MEMORY_H
#define MEMORY_H

#include "../config.h"		// Configurations

#include "memory_hp.h"		// Using Hazard Pointers

#include "memory_ebr2.h"	// Using Epoch-Based Reclamation (Scott's Group, PPoPP'18)

#include "memory_dang.h"	// Using Data Locality

#include "memory_dang2.h"	// Using Hazard-Pointers-like Reclamation

#include "memory_dang3.h"	// Using no Reclamation

#endif /* MEMORY_H */
