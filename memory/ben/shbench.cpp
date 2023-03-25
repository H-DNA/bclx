#include <cstdint>		// uint64_t...
#include <cmath>		// exp2l...
#include <random>		// std::default_random_engine
#include <bclx/bclx.hpp>	// BCL::init...

/* Benchmark-specific tuning parameters */
const uint64_t	BLOCK_SIZE_MIN	= 1;
const uint64_t	BLOCK_SIZE_MAX	= 1024;
const uint64_t	NUM_ITERS	= 7 * exp2l(20);
const uint64_t	ARRAY_SIZE	= 100;

// debugging
//const uint64_t  NUM_ITERS       = 8;
//const uint64_t	ARRAY_SIZE	= 2;

int main()
{		
	BCL::init();	// initialize the PGAS runtime
	uint64_t				size;
	std::default_random_engine		generator;
	std::uniform_int_distribution<uint64_t>	distribution(BLOCK_SIZE_MIN, BLOCK_SIZE_MAX); 
	bclx::gptr<void>			ptr[ARRAY_SIZE];
	bclx::timer				tim;
	bclx::memory				mem;

	bclx::barrier_sync(); // synchronize
	tim.start();	// start the timer
	for (uint64_t i = 0; i < NUM_ITERS / BCL::nprocs(); ++i)
	{
		for (uint64_t j = 0; j < ARRAY_SIZE; ++j)
		{
			size = distribution(generator);
			ptr[j] = mem.malloc(size);

			// debugging
			//printf("[%lu]malloc: ptr = <%u, %u>, size = %lu\n",
			//		BCL::rank(), ptr[j].rank, ptr[j].ptr, size);
		}
		for (uint64_t j = 0; j < ARRAY_SIZE; ++j)
			mem.free(ptr[j]);
	}
	tim.stop();	// stop the timer
	double elapsed_time = tim.get();	// get the elapsed time
	bclx::barrier_sync();	// synchronize

	double total_time = bclx::reduce(elapsed_time, bclx::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == bclx::MASTER_UNIT)
	{
		uint64_t num_ops_per_unit = ARRAY_SIZE + BCL::nprocs() * 2 * NUM_ITERS;
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tShbench\t\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit) \t\t*\n", NUM_ITERS / BCL::nprocs() * 2 * ARRAY_SIZE);
		printf("*\tARRAY_SIZE\t:\t%lu\t\t\t\t*\n", ARRAY_SIZE);
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu-%lu (bytes) \t\t\t*\n", BLOCK_SIZE_MIN, BLOCK_SIZE_MAX);
		printf("*\tMEM_ALLOC\t:\t%s\t\t\t*\n", mem.get_name());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t\t*\n", BCL::nprocs() * num_ops_per_unit / total_time);
		printf("*****************************************************************\n");
	}

        // debugging
        #ifdef  DEBUGGING
        if (BCL::rank() == 0)
        {
                //printf("[%lu]cnt_buffers = %lu\n", BCL::rank(), bclx::cnt_buffers);
                printf("[%lu]cnt_ncontig = %lu\n", BCL::rank(), bclx::cnt_ncontig);
                //printf("[%lu]cnt_ncontig2 = %lu\n", BCL::rank(), bclx::cnt_ncontig2);
                printf("[%lu]cnt_contig = %lu\n", BCL::rank(), bclx::cnt_contig);
                printf("[%lu]cnt_bcl = %lu\n", BCL::rank(), bclx::cnt_bcl);
                printf("[%lu]cnt_free = %lu\n", BCL::rank(), bclx::cnt_free);
		//printf("[%lu]cnt_rfree = %lu\n", BCL::rank(), bclx::cnt_rfree);
        }
        #endif

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
