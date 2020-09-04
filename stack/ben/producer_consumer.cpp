#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"
#include "../../lib/ta.h"

using namespace dds;
using namespace dds::ts;

int main()
{
        uint32_t 	i,
			value;
	clock_t		start,
			end;
        double		cpu_time_used,
			total_time;

        BCL::init();

	uint32_t	num_ops = ELEM_PER_UNIT / BCL::nprocs();

	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tProducer-consumer\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_OPS\t\t:\t%u (ops/unit)\t*\n", num_ops);
                printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
	}

	if (BCL::nprocs() % 2 != 0)
	{
		printf("ERROR: The number of units must be even!\n");
		return -1;
	}

        stack<uint32_t> myStack;

	BCL::barrier();

	for (i = 0; i < num_ops / 2; ++i)
	{
		//debugging
		#ifdef DEBUGGING
			printf ("[%lu]%u\n", BCL::rank(), i);
		#endif

		value = i;
		myStack.push_fill(value);
	}

        BCL::barrier();

	end = 0;
	if (BCL::rank() % 2 == 0)
	{
		for (i = 0; i < num_ops; ++i)
		{
			//debugging
			#ifdef DEBUGGING
               			printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			value = i;
			start = clock();
			myStack.push(value);
			end += (clock() - start);
			usleep(WORKLOAD);
		}
	}
	else //if (BCL::rank() % 2 != 0)
		for (i = 0; i < num_ops; ++i)
		{
                        //debugging
			#ifdef DEBUGGING
                        	printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			start = clock();
			myStack.pop(value);
			end += (clock() - start);
			usleep(WORKLOAD);
		}

        BCL::barrier();

        cpu_time_used = ((double) end) / CLOCKS_PER_SEC;
	total_time = BCL::reduce(cpu_time_used, MASTER_UNIT, BCL::max<double>{});
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
		double          node_time;
		ta::na          na;

		MPI_Reduce(&succ_cs, &total_succ_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_cs, &total_fail_cs, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&succ_ea, &total_succ_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&fail_ea, &total_fail_ea, 1, MPI_UINT64_T, MPI_SUM, MASTER_UNIT, na.nodeComm);
		MPI_Reduce(&cpu_time_used, &node_time, 1, MPI_DOUBLE, MPI_MAX, MASTER_UNIT, na.nodeComm);
		if (na.rank == MASTER_UNIT)
			printf("[Node %d]Execution time = %f seconds, %lu, %lu, %lu, %lu\n", na.node_id,
					node_time, total_succ_cs, total_fail_cs, total_succ_ea, total_fail_ea);
	#endif

	BCL::finalize();

	return 0;
}
