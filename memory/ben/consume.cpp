#include <cstdint>		// uint64_t...
#include <bcl/bcl.hpp>		// BCL::init...
#include "../inc/memory.h"	// dds::memory...

/* Macros */
using namespace dds;
#ifdef	MEM_HP
	using namespace hp;
#endif

/* Benchmark-specific tuning parameters */
const uint64_t	NUM_ITERS = 5000;

int main()
{		
	BCL::init();

	gptr<uint64_t>		ptr;
	memory<uint64_t>	mem;

	uint64_t num_ops = NUM_ITERS * (BCL::nprocs() - 1) * 2;

	double start = MPI_Wtime();

	for (uint64_t i = 0; i < NUM_ITERS; ++i)
	{
		if (BCL::rank() == MASTER_UNIT)
		{
			for (uint64_t j = 0; j < BCL::nprocs() - 1; ++j)
				ptr = mem.malloc();
			BCL::broadcast(ptr, MASTER_UNIT);	// synchronize
		}
		else // if (BCL::rank() != MASTER_UNIT)
		{
			BCL::broadcast(ptr, MASTER_UNIT);
			ptr += BCL::rank();
			mem.free(ptr);
		}
	}

	double end = MPI_Wtime();

	double elapsed_time = end - start;

	double total_time = BCL::reduce(elapsed_time, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tConsume\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit)\t*\n", num_ops);
		//printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
		//printf("*\tSTACK\t\t:\t%s\t\t\t*\n", stack_name.c_str());
		printf("*\tMEMORY\t\t:\t%s\t\t\t*\n", mem_manager.c_str());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", TOTAL_OPS / total_time);
		printf("*********************************************************\n");
	}

	BCL::finalize();

	return 0;
}
