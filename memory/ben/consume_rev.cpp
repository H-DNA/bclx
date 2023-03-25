#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// BCL::init...

/* Benchmark-specific tuning parameters */
const uint64_t	BLOCK_SIZE	= 64;
const uint64_t	NUM_ITERS	= 5000;

int main()
{		
	BCL::init();	// initialize the PGAS runtime

	bclx::gptr<void>	ptr_malloc[BCL::nprocs()],
				ptr_free[BCL::nprocs()];
	bclx::gptr<char>	tmp_ptr;
	char			tmp_val[BLOCK_SIZE];
	bclx::topology		topo;
	bclx::timer		tim;
	bclx::memory		mem;

	for (uint64_t i = 0; i < NUM_ITERS; ++i)
	{
		bclx::barrier_sync();	// synchronize
		tim.start();	// start the timer
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
		{
			ptr_malloc[j] = mem.malloc(BLOCK_SIZE);
			tmp_ptr = {ptr_malloc[j].rank, ptr_malloc[j].ptr};
			bclx::rput_sync(tmp_val, tmp_ptr, BLOCK_SIZE);	// produce
		}
		tim.stop();	// stop the timer

		/* exchange the global pointers */
		bclx::alltoall(ptr_malloc, ptr_free);

		bclx::barrier_sync();	// synchronize
		tim.start();	// start the timer
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
		{
			tmp_ptr = {ptr_free[j].rank, ptr_free[j].ptr};
			bclx::rget_sync(tmp_ptr, tmp_val, BLOCK_SIZE);	// consume
			mem.free(ptr_free[j]);
		}
		tim.stop();	// stop the timer
	}

	double elapsed_time = tim.get();	// get the elapsed time
	bclx::barrier_sync();	// synchronize

	double total_time = bclx::reduce(elapsed_time, bclx::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == bclx::MASTER_UNIT)
	{
		uint64_t num_ops_per_unit = BCL::nprocs() * 2 * NUM_ITERS;
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tConsume-rev\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit) \t\t*\n", num_ops_per_unit);
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu (bytes) \t\t\t*\n", BLOCK_SIZE);
		printf("*\tMEM_ALLOC\t:\t%s\t\t\t*\n", mem.get_name());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t\t*\n", BCL::nprocs() * num_ops_per_unit / total_time);
		printf("*****************************************************************\n");
	}

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
