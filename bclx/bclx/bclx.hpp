#pragma once

#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>

#ifdef	SHMEM
	#include <bclx/backends/shmem/backend.hpp>
#elif	GASNET_EX
	#include <bclx/backends/gasnet-ex/backend.hpp>
#elif	UPCXX
	#include <bclx/backends/upcxx/backend.hpp>
#else	// MPI
	#include <bclx/backends/mpi/backend.hpp>
#endif

#include <bclx/core/comm.hpp>

#ifdef	HYDRALLOC_1
	#include <bclx/core/alloc/hydralloc/1/alloc.hpp>
#elif	HYDRALLOC_2
	#include <bclx/core/alloc/hydralloc/2/alloc.hpp>
#elif	HYDRALLOC_3
	#include <bclx/core/alloc/hydralloc/3/alloc.hpp>
#else	// IdeAlloc
	#include <bclx/core/alloc/idealloc/alloc.hpp>
#endif

#include <bclx/core/util.hpp>


