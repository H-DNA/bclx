#include <thread>
#include <chrono>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"
#include "../../lib/ta.h"

using namespace dds;
using namespace dds::ts;

int main()
{
        uint32_t	i,
			value;
	uint64_t	num_ops;
        double		start,
			end,
			elapsed_time,
			total_time;

	BCL::init();

        stack<uint32_t> myStack;
	num_ops = ELEMS_PER_UNIT / BCL::nprocs();

	start = MPI_Wtime();

	for (i = 0; i < num_ops / 2; ++i)
	{
		//debugging
		#ifdef DEBUGGING
			printf ("[%lu]%u\n", BCL::rank(), i);
		#endif

		myStack.push(i);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));

                myStack.pop(value);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));
	}

	end = MPI_Wtime();

	elapsed_time = (end - start) - ((double) num_ops * WORKLOAD) / 1000000;

        total_time = BCL::reduce(elapsed_time, MASTER_UNIT, BCL::max<double>{});
        if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tSequential-alternating\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%lu (ops/unit)\t\t*\n", num_ops);
		printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
		printf("*\tSTACK\t\t:\t%s\t\t\t*\n", stack_name.c_str());
		printf("*\tMEMORY\t\t:\t%s\t\t\t*\n", mem_manager.c_str());
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
                printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", ELEMS_PER_UNIT / total_time);
                printf("*********************************************************\n");
	}

        //tracing
        #ifdef  TRACING
		uint64_t	total_elem_re,
				total_succ_cs,
				total_fail_cs,
				total_succ_ea,
				total_fail_ea;
		double		node_time,
				total_fail_time;
        	ta::na          na;

		if (na.node_num == 1)
			printf("[Proc %lu]%f (s), %f (s), %lu, %lu, %lu, %lu, %lu\n", BCL::rank(),
				elapsed_time, fail_time, succ_cs, fail_cs, succ_ea, fail_ea, elem_re);

		MPI_Reduce(&elem_re, &total_elem_re, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&succ_cs, &total_succ_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_cs, &total_fail_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&succ_ea, &total_succ_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_ea, &total_fail_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&elapsed_time, &node_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_time, &total_fail_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, na.nodeComm);
                if (na.rank == MASTER_UNIT)
                        printf("[Node %d]%f (s), %f (s), %lu, %lu, %lu, %lu, %lu\n", na.node_id, node_time,
				total_fail_time, total_succ_cs, total_fail_cs, total_succ_ea, total_fail_ea, total_elem_re);
        #endif

	BCL::finalize();

	return 0;
}
