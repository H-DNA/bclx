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
        double		start,
			total_time;

	//tracing
	#ifdef	TRACING
		double	end;
	#endif

	BCL::init();

        uint32_t        num_ops = ELEM_PER_UNIT / BCL::nprocs();

	if (BCL::rank() == MASTER_UNIT)
	{
                printf("*********************************************************\n");
                printf("*\tBENCHMARK\t:\tSequential-alternating\t*\n");
                printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
                printf("*\tNUM_OPS\t\t:\t%u (ops/unit)\t*\n", num_ops);
                printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
	}

        stack<uint32_t> myStack;

	BCL::barrier();
	start = MPI_Wtime();

	for (i = 0; i < num_ops / 2; ++i)
	{
		//debugging
		#ifdef DEBUGGING
			printf ("[%lu]%u\n", BCL::rank(), i);
		#endif

		value = i;
		myStack.push(value);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));

                myStack.pop(value);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));
	}

	//tracing
	#ifdef	TRACING
		end = MPI_Wtime();
	#endif

	BCL::barrier();
	total_time = (MPI_Wtime() - start) - ((double) num_ops * WORKLOAD) / 1000000;

        total_time = BCL::reduce(total_time, MASTER_UNIT, BCL::max<double>{});
        if (BCL::rank() == MASTER_UNIT)
	{
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
                printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", ELEM_PER_UNIT / total_time);
                printf("*********************************************************\n");
	}

        //tracing
        #ifdef  TRACING
		uint64_t	total_succ_cs,
				total_fail_cs,
				total_succ_ea,
				total_fail_ea;
		double		cpu_time_used,
				node_time,
				total_fail_time;
        	ta::na          na;

		cpu_time_used = (end - start) - ((double) num_ops * WORKLOAD) / 1000000;

		if (na.node_num == 1)
			printf("[Proc %lu]%f (s), %f (s), %lu, %lu, %lu, %lu\n", BCL::rank(),
					cpu_time_used, fail_time, succ_cs, fail_cs, succ_ea, fail_ea);

		MPI_Reduce(&succ_cs, &total_succ_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_cs, &total_fail_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&succ_ea, &total_succ_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_ea, &total_fail_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&cpu_time_used, &node_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_time, &total_fail_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, na.nodeComm);
                if (na.rank == MASTER_UNIT)
                        printf("[Node %d]%f (s), %f (s), %lu, %lu, %lu, %lu\n", na.node_id, node_time,
					total_fail_time, total_succ_cs, total_fail_cs, total_succ_ea, total_fail_ea);
        #endif

	BCL::finalize();

	return 0;
}
