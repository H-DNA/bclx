#include <cstdint>		// uint64_t...
#include <cmath>		// exp2l...
#include <bclx/bclx.hpp>	// bclx::gptr...
#include "../inc/memory.h"	// dds::memory...

/* Macros */
#ifdef		MEM_HP
	using namespace dds::hp;
#elif defined	MEM_DANG2
	using namespace dds::dang2;
#elif defined	MEM_DANG3
	using namespace dds::dang3;
#elif defined	MEM_DANG4
	using namespace dds::dang4;
#elif defined	MEM_DANG5
	using namespace dds::dang5;
#elif defined	MEM_TRIAL
	using namespace dds::trial;
#else		// No Memory Reclamation
	using namespace dds::nmr;
#endif

/* Benchmark-specific tuning parameters */
const uint64_t	NUM_ITERS	= 5000;
const uint64_t	NUM_BLOCKS	= 7 * exp2l(15);

/* 64-byte block structure */
struct block
{
	uint64_t	a1;
	uint64_t	a2;
	uint64_t	a3;
	uint64_t	a4;
	uint64_t	a5;
	uint64_t	a6;
	uint64_t	a7;
	uint64_t	a8;
};

int main()
{
	BCL::init();	// initialize the PGAS runtime

	const uint64_t	NUM_OPS_PER_UNIT = NUM_BLOCKS / BCL::nprocs();
	gptr<block>	ptr[NUM_OPS_PER_UNIT];
	timer		tim;
	memory<block>	mem;

	bclx::barrier_sync();	// synchronize
	tim.start();	// start the timer
	for (uint64_t i = 0; i < NUM_ITERS; ++i)
	{
		for (uint64_t j = 0; j < NUM_OPS_PER_UNIT; ++j)
			ptr[j] = mem.malloc();
		for (uint64_t j = 0; j < NUM_OPS_PER_UNIT; ++j)
			mem.free(ptr[j]);
	}
	tim.stop();	// stop the timer
	double elapsed_time = tim.get();	// calculate the elapsed time
	bclx::barrier_sync();	// synchronize

	double total_time = bclx::reduce(elapsed_time, dds::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == dds::MASTER_UNIT)
	{
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tThreadtest\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit)\t\t*\n", 2 * NUM_ITERS * NUM_OPS_PER_UNIT);
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu (bytes)\t\t\t*\n", sizeof(block));
		printf("*\tMEM_MANGER\t:\t%s\t\t\t\t*\n", dds::mem_manager.c_str());
		printf("*\tEXEC_TIME\t:\t%f (s)\t\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", 2 * NUM_ITERS * NUM_BLOCKS / total_time);
		printf("*****************************************************************\n");
	}

        // debugging
        #ifdef  DEBUGGING
        if (BCL::rank() == 0)
        {
                printf("[%lu]cnt_buffers = %lu\n", BCL::rank(), dds::cnt_buffers);
                printf("[%lu]cnt_ncontig = %lu\n", BCL::rank(), dds::cnt_ncontig);
		printf("[%lu]cnt_ncontig2 = %lu\n", BCL::rank(), dds::cnt_ncontig2);
                printf("[%lu]cnt_contig = %lu\n", BCL::rank(), dds::cnt_contig);
                printf("[%lu]cnt_pool = %lu\n", BCL::rank(), dds::cnt_pool);
		printf("[%lu]cnt_free = %lu\n", BCL::rank(), dds::cnt_free);
		printf("[%lu]cnt_rfree = %lu\n", BCL::rank(), dds::cnt_rfree);
        }
        #endif

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
