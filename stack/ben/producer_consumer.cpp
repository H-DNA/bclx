#include <thread>			// std::this_thread...
#include <chrono>			// std::chrono...
#include <cstdint>			// uint32_t...
#include <bclx/bclx.hpp>		// BCL::init...
#include "../inc/stack.h"		// dds::stack...

using namespace dds;
using namespace dds::ts;

int main()
{
        uint32_t 	i;
	uint32_t	value;
	uint64_t	num_ops;
	double		elapsed_time,
			total_time;
	bclx::timer	tim;

        BCL::init();

	if (BCL::nprocs() % 2 != 0)
	{
		printf("ERROR: The number of units must be even!\n");
		return -1;
	}

        stack<uint32_t> myStack(TOTAL_OPS / 2);
	num_ops = TOTAL_OPS / BCL::nprocs();

	tim.start();	// start the timer

	if (BCL::rank() % 2 == 0)
	{
		for (i = 0; i < num_ops; ++i)
		{
			// debugging
			#ifdef DEBUGGING
               			printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			myStack.push(i);
			std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));
		}
	}
	else // if (BCL::rank() % 2 != 0)
		for (i = 0; i < num_ops; ++i)
		{
                        // debugging
			#ifdef DEBUGGING
                        	printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			myStack.pop(value);
			std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));
		}

	tim.stop();	// stop the timer

	elapsed_time = tim.get() - ((double) num_ops * WORKLOAD) / 1000000;

	total_time = bclx::reduce(elapsed_time, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tProducer-consumer\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit)\t\t*\n", num_ops);
		printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
		printf("*\tSTACK\t\t:\t%s\t\t\t*\n", stack_name.c_str());
		printf("*\tMEMORY\t\t:\t%s\t\t\t*\n", mem_manager.c_str());
		printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", TOTAL_OPS / total_time);
                printf("*********************************************************\n");
	}

	//tracing
	#ifdef  TRACING
		uint64_t	node_elem_ru,
				node_elem_rc,
				node_succ_cs,
				node_fail_cs,
				node_succ_ea,
				node_fail_ea;
		double		node_elapsed_time,
				node_fail_time;
		bclx::topology	topo;

		if (topo.node_num == 1)
			printf("[Proc %lu]%f (s), %f (s), %lu, %lu, %lu, %lu, %lu, %lu\n",
					BCL::rank(), elapsed_time, fail_time,
					succ_cs, fail_cs,
					succ_ea, fail_ea,
					elem_rc, elem_ru);

		MPI_Reduce(&elem_ru, &node_elem_ru, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&elem_rc, &node_elem_rc, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&succ_cs, &node_succ_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&fail_cs, &node_fail_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&succ_ea, &node_succ_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&fail_ea, &node_fail_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&elapsed_time, &node_elapsed_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, topo.nodeComm);
		MPI_Reduce(&fail_time, &node_fail_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, topo.nodeComm);
		if (topo.rank == MASTER_UNIT)
			printf("[Node %d]%f (s), %f (s), %lu, %lu, %lu, %lu, %lu, %lu\n",
					topo.node_id, node_elapsed_time, node_fail_time,
					node_succ_cs, node_fail_cs,
					node_succ_ea, node_fail_ea,
					node_elem_rc, node_elem_ru);
		if (topo.node_num > 1)
		{
			uint64_t total_elem_ru = bclx::reduce(elem_ru, MASTER_UNIT, BCL::sum<uint64_t>{});
			uint64_t total_elem_rc = bclx::reduce(elem_rc, MASTER_UNIT, BCL::sum<uint64_t>{});
			uint64_t total_succ_cs = bclx::reduce(succ_cs, MASTER_UNIT, BCL::sum<uint64_t>{});
			uint64_t total_fail_cs = bclx::reduce(fail_cs, MASTER_UNIT, BCL::sum<uint64_t>{});
			uint64_t total_succ_ea = bclx::reduce(succ_ea, MASTER_UNIT, BCL::sum<uint64_t>{});
			uint64_t total_fail_ea = bclx::reduce(fail_ea, MASTER_UNIT, BCL::sum<uint64_t>{});
			if (BCL::rank() == MASTER_UNIT)
				printf("[TOTAL]%lu, %lu, %lu, %lu, %lu, %lu\n",
						total_succ_cs, total_fail_cs,
						total_succ_ea, total_fail_ea,
						total_elem_rc, total_elem_ru);
		}
	#endif

	BCL::finalize();

	return 0;
}
