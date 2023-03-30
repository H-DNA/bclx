#include <cstdint>		// uint64_t...
#include <random>		// std::default_random_engine
#include <bclx/bclx.hpp>	// BCL::init...

/* Benchmark-specific tuning parameters */
const uint64_t	BLOCK_SIZE_MIN	= 8;
const uint64_t	BLOCK_SIZE_MAX	= 512;
const uint64_t	NUM_ITERS	= 5000;
const uint64_t	ARRAY_SIZE	= 100;

int main()
{		
	BCL::init();	// initialize the PGAS runtime

	uint64_t				index;
	uint64_t				size;
	std::default_random_engine		generator;
	std::uniform_int_distribution<uint64_t> distribution_index(0, ARRAY_SIZE - 1);
	std::uniform_int_distribution<uint64_t>	distribution_size(BLOCK_SIZE_MIN, BLOCK_SIZE_MAX); 
	bclx::gptr<void>			array[ARRAY_SIZE];
	bclx::timer				tim;
	bclx::memory				mem;

	bclx::barrier_sync(); // synchronize
	tim.start();	// start the timer
	for (uint64_t i = 0; i < ARRAY_SIZE; ++i)
	{
		size = distribution_size(generator);
		array[i] = mem.malloc(size);
	}
	tim.stop();	// stop the timer

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		bclx::barrier_sync();	// synchronize
		tim.start();	// start the timer
		for (uint64_t j = 0; j < NUM_ITERS; ++j)
		{
			index = distribution_index(generator);
			mem.free(array[index]);

			size = distribution_size(generator);
			array[index] = mem.malloc(size);
		}
		tim.stop();	// stop the timer
	
		/* exchange the global pointers */
		bclx::send(array, (BCL::rank() + 1) % BCL::nprocs(), ARRAY_SIZE);
		bclx::recv(array, (BCL::rank() + BCL::nprocs() - 1) % BCL::nprocs(), ARRAY_SIZE);
	}

	double elapsed_time = tim.get();	// get the elapsed time
	bclx::barrier_sync();	// synchronize

	double total_time = bclx::reduce(elapsed_time, bclx::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == bclx::MASTER_UNIT)
	{
		uint64_t num_ops_per_unit = ARRAY_SIZE + BCL::nprocs() * 2 * NUM_ITERS;
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tLarson\t\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit) \t\t*\n", num_ops_per_unit);
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
        if (BCL::rank() == bclx::MASTER_UNIT)
        {
                //printf("[%lu]cnt_buffers = %lu\n", BCL::rank(), bclx::cnt_buffers);
                printf("[%lu]cnt_ncontig = %lu\n", BCL::rank(), bclx::cnt_ncontig);
                printf("[%lu]cnt_ncontig2 = %lu\n", BCL::rank(), bclx::cnt_ncontig2);
                printf("[%lu]cnt_contig = %lu\n", BCL::rank(), bclx::cnt_contig);
                printf("[%lu]cnt_bcl = %lu\n", BCL::rank(), bclx::cnt_bcl);
                printf("[%lu]cnt_lfree = %lu\n", BCL::rank(), bclx::cnt_lfree);
		printf("[%lu]cnt_rfree = %lu\n", BCL::rank(), bclx::cnt_rfree);
        }
        #endif

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
