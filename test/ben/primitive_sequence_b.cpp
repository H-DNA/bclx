#include <cstdint>		// uint32_t...
#include <bcl/bcl.hpp>		// BCL::init...
#include "../config.h"		// TOTAL_OPS...

using namespace dds;

int main()
{
        uint32_t 	i;
	gptr<gptr<int>>	ptr;
	double		start,
			end,
			elapsed_time,
			total_time;

	// Initialize PGAS programming model
        BCL::init();

	// Check if the program is valid
	if (BCL::nprocs() != 2)
	{
		printf("ERROR: The number of units must be 2!\n");
		return -1;
	}

	// Initialize shared global memory
	if (BCL::rank() == MASTER_UNIT)
	{
		ptr = BCL::alloc<gptr<int>>(NUM_OPS);
	}

	// Barrier
	ptr = BCL::broadcast(ptr, MASTER_UNIT);

	// Communication b
	start = MPI_Wtime();	// Start timing
	if (BCL::rank() != MASTER_UNIT)
	{
		for (i = 0; i < NUM_OPS - 1; ++i)
		{
			BCL::rput_async({i, i}, ptr);
			++ptr;
		}
		BCL::rput_sync({i, i}, ptr);
	}
	end = MPI_Wtime();	// Stop timing
	elapsed_time = end - start;

	total_time = BCL::reduce(elapsed_time, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tPrimitive Sequence (b)\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (puts)\t\t*\n", NUM_OPS);
		printf("*\tTOTAL_TIME\t:\t%f (s)\t\t*\n", total_time);
                printf("*********************************************************\n");
	}

	// Finalize PGAS programming model
	BCL::finalize();

	return 0;
}
