#include <vector>		// std::vector...
#include <cstdint>		// uint32_t...
#include <bclx/bclx.hpp>	// BCL::init...
#include "../config.h"		// TOTAL_OPS...

int main()
{
	using namespace bclx;
	using namespace dds;

        uint32_t 	i,
			j;
	gptr<gptr<int>>	ptr,
			ptr_tmp;
	double		start,
			end,
			elapsed_time = 0,
			elapsed_time2 = 0,
			elapsed_time3 = 0,
			total_time,
			total_time2,
			total_time3;

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
		ptr = BCL::alloc<gptr<int>>(NUM_OPS);

	// Broadcast
	ptr = BCL::broadcast(ptr, MASTER_UNIT);

	for (i = 0; i < NUM_ITERS; ++i)
	{
		/* Communication 1 */
		bclx::barrier_sync();	// Synchronize
       		start = MPI_Wtime();	// Start timing
		if (BCL::rank() != MASTER_UNIT)
		{
			ptr_tmp = ptr;
			for (j = 0; j < NUM_OPS - 1; ++j)
			{
				rput_sync({i, j}, ptr_tmp);
				++ptr_tmp;
			}
			rput_sync({i, j}, ptr_tmp);
		}
		end = MPI_Wtime();	// Stop timing
		elapsed_time += end - start;
		/**/

		/* Communication 2 */
		bclx::barrier_sync();	// Synchronize
		start = MPI_Wtime();	// Start timing
		if (BCL::rank() != MASTER_UNIT)
		{
			ptr_tmp = ptr;
			for (j = 0; j < NUM_OPS - 1; ++j)
			{
				rput_async({i, j}, ptr_tmp);
				++ptr_tmp;
			}
			rput_sync({i, j}, ptr_tmp);
		}
		end = MPI_Wtime();	// Stop timing
		elapsed_time2 += end - start;
		/**/

		/* Communication 3 */
		std::vector<int64_t>	disp;
		std::vector<gptr<int>>	buffer;
		ptr_tmp = ptr;
		for (j = 0; j < NUM_OPS; ++j)
		{
			disp.push_back(int64_t(ptr_tmp.ptr) - int64_t(ptr.ptr));
			buffer.push_back({i, j});
			++ptr_tmp;
		}
		rll_t*		rll = new rll_t(disp);
		gptr<rll_t>	base = {ptr.rank, ptr.ptr};
		bclx::barrier_sync();	// Synchronize
		start = MPI_Wtime();	// Starting timing
		if (BCL::rank() != MASTER_UNIT)
			rput_sync(buffer, base, *rll);
		end = MPI_Wtime();	// Stop timing
		elapsed_time3 += end - start;

		delete rll;
	}

	bclx::barrier_sync();	// Synchronize

	total_time = bclx::reduce(elapsed_time, MASTER_UNIT, BCL::max<double>{});
	total_time2 = bclx::reduce(elapsed_time2, MASTER_UNIT, BCL::max<double>{});
	total_time3 = bclx::reduce(elapsed_time3, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tPrimitive Sequence\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t*\n", NUM_ITERS);
		printf("*\tNUM_OPS\t\t:\t%lu (puts)\t\t*\n", NUM_OPS);
		printf("*\tTOTAL_TIME\t:\t%f (s)\t\t*\n", total_time / NUM_ITERS);
		printf("*\tTOTAL_TIME2\t:\t%f (s)\t\t*\n", total_time2 / NUM_ITERS);
		printf("*\tTOTAL_TIME3\t:\t%f (s)\t\t*\n", total_time3 / NUM_ITERS);
                printf("*********************************************************\n");
	}

	// Finalize PGAS programming model
	BCL::finalize();

	return 0;
}
