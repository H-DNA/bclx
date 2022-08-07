#include <cstdint>		// uint64_t...
#include <cmath>		// exp2l...
#include <bclx/bclx.hpp>	// bclx::gptr...
#include "../inc/memory.h"	// dds::memory...

/* Macros */
#ifdef	MEM_HP
	using namespace dds::hp;
#endif

/* Benchmark-specific tuning parameters */
const uint64_t	NUM_ITERS	= exp2l(30);

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

	gptr<block>	ptr;
	timer		tim;
	memory<block>	mem;

	barrier_sync();	// synchronize
	tim.start();	// start the timer
	for (uint64_t i = 0; i < NUM_ITERS / BCL::nprocs(); ++i)
	{
		ptr = mem.malloc();
		mem.free(ptr);
	}
	tim.stop();	// stop the timer
	double elapsed_time = tim.get();	// calculate the elapsed time
	barrier_sync();	// synchronize

	double total_time = reduce(elapsed_time, dds::MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == dds::MASTER_UNIT)
	{
		printf("*****************************************************************\n");
		printf("*\tBENCHMARK\t:\tlinux-scalability\t\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit)\t\t*\n", 2 * NUM_ITERS / BCL::nprocs());
		printf("*\tNUM_ITERS\t:\t%lu\t\t\t*\n", NUM_ITERS);
		printf("*\tBLOCK_SIZE\t:\t%lu (bytes)\t\t\t*\n", sizeof(block));
		printf("*\tMEM_MANGER\t:\t%s\t\t\t\t*\n", dds::mem_manager.c_str());
		printf("*\tEXEC_TIME\t:\t%f (s)\t\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t\t*\n", 2 * NUM_ITERS / total_time);
		printf("*****************************************************************\n");
	}

	BCL::finalize();	// finalize the PGAS runtime

	return 0;
}
