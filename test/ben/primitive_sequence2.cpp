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
			elapsed_time_comm,
			elapsed_time_comm2,
			total_time_comm,
			total_time_comm2;

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
		ptr = BCL::alloc<gptr<int>>(1);
	ptr = BCL::broadcast(ptr, MASTER_UNIT);

	// Communication 1
	BCL::barrier();		// Barrier
       	start = MPI_Wtime();	// Start timing
	if (BCL::rank() == 1)
	{
		for (i = 0; i < TOTAL_OPS; ++i)
			BCL::rput_sync({i, i}, ptr);
	}
	end = MPI_Wtime();	// Stop timing
	elapsed_time_comm = end - start;

	// Communication 2
	BCL::barrier();		// Barrier
	start = MPI_Wtime();	// Start timing
	if (BCL::rank() == 1)
	{
		for (i = 0; i < TOTAL_OPS - 1; ++i)
			BCL::rput_async({i, i}, ptr);
		BCL::rput_sync({i, i}, ptr);
	}
	end = MPI_Wtime();	// Stop timing
	elapsed_time_comm2 = end - start;

	total_time_comm = BCL::reduce(elapsed_time_comm, MASTER_UNIT, BCL::max<double>{});
	total_time_comm2 = BCL::reduce(elapsed_time_comm2, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tPrimitive Sequence 2\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_PUTS_2\t:\t%lu (ops)\t\t*\n", TOTAL_OPS);
		printf("*\tTOTAL_TIME\t:\t%f (s)\t\t*\n", total_time_comm);
		printf("*\tTOTAL_TIME2\t:\t%f (s)\t\t*\n", total_time_comm2);
                printf("*********************************************************\n");
	}

	// Finalize PGAS programming model
	BCL::finalize();

	return 0;
}
