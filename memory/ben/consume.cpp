#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// BCL::init...
#include "../inc/memory.h"	// dds::memory...

/* Macros */
#ifdef		MEM_HP
	using namespace dds::hp;
#elif defined	MEM_DANG2
	using namespace dds::dang2;
#else	// No Memory Reclamation
	using namespace dds::nmr;
#endif

/* Benchmark-specific tuning parameters */
const uint64_t	NUM_ITERS = 5000;

/* 64-byte block structure */
struct block
{
        uint64_t        a1;
        uint64_t        a2;
        uint64_t        a3;
        uint64_t        a4;
        uint64_t        a5;
        uint64_t        a6;
        uint64_t        a7;
        uint64_t        a8;
};

int main()
{		
	BCL::init();	// initialize the PGAS runtime

	gptr<block>	ptr[BCL::nprocs()];
	timer		tim;
	memory<block>	mem;

	for (uint64_t i = 0; i < NUM_ITERS; ++i)
	{
		barrier_sync();	// synchronize
		tim.start();	// start the timer
		if (BCL::rank() == dds::MASTER_UNIT)
			for (uint64_t j = 0; j < BCL::nprocs(); ++j)
				ptr[j] = mem.malloc();
		tim.stop();	// stop the timer

		ptr[BCL::rank()] = scatter(ptr, dds::MASTER_UNIT);

		barrier_sync();	// synchronize
		tim.start();	// start the timer
		mem.free(ptr[BCL::rank()]);
		tim.stop();	// stop the timer
	}

	double elapsed_time = tim.get();	// get the elapsed time
	barrier_sync();	// synchronize

	double total_time = reduce(elapsed_time, dds::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == dds::MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tConsume\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (total) \t\t*\n", 2 * NUM_ITERS * BCL::nprocs());
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu (bytes) \t\t*\n", sizeof(block));
		printf("*\tMEM_MANAGER\t:\t%s\t\t\t*\n", dds::mem_manager.c_str());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", 2 * NUM_ITERS * BCL::nprocs() / total_time);
		printf("*********************************************************\n");
	}

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
