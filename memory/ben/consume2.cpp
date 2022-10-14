#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// BCL::init...
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
#else		// No Memory Reclamation
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

	gptr<block>	ptr_malloc[BCL::nprocs()],
			ptr_free[BCL::nprocs()];
	bclx::topology	topo;
	timer		tim;
	memory<block>	mem;

	for (uint64_t i = 0; i < NUM_ITERS; ++i)
	{
		bclx::barrier_sync();	// synchronize
		tim.start();	// start the timer
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
		{
			ptr_malloc[j] = mem.malloc();
			for (uint64_t k = 0; k < 2; ++k)
				bclx::rput_sync({j, j, j, j, j, j, j, j}, ptr_malloc[j]);	// produce
		}
		tim.stop();	// stop the timer

		/* exchange the global pointers */
		bclx::alltoall(ptr_malloc, ptr_free);

		bclx::barrier_sync();	// synchronize
		tim.start();	// start the timer
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
		{
			bclx::rget_sync(ptr_free[j]);	// consume
			mem.free(ptr_free[j]);
		}
		tim.stop();	// stop the timer
	}

	double elapsed_time = tim.get();	// get the elapsed time
	bclx::barrier_sync();	// synchronize

	double total_time = bclx::reduce(elapsed_time, dds::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == dds::MASTER_UNIT)
	{
		uint64_t num_ops_per_unit = BCL::nprocs() * 2 * NUM_ITERS;
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tConsume2\t\t\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit) \t\t*\n", num_ops_per_unit);
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu (bytes) \t\t\t*\n", sizeof(block));
		printf("*\tMEM_MANAGER\t:\t%s\t\t\t\t*\n", dds::mem_manager.c_str());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", BCL::nprocs() * num_ops_per_unit / total_time);
		printf("*****************************************************************\n");
	}

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
