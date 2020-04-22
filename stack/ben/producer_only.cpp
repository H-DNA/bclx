#include <unistd.h>
#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"

using namespace dds;
using namespace dds::bs2;

int main()
{
        uint32_t 	i,
			value;
        clock_t 	start,
			end;
        double 		cpu_time_used,
			total_time;

        BCL::init();

	uint32_t	num_ops = ELEM_PER_UNIT / BCL::nprocs();

	if (BCL::rank() == MASTER_UNIT)
	{
                printf("*********************************************************\n");
                printf("*\tBENCHMARK\t:\tProducer-only\t\t*\n");
                printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
                printf("*\tNUM_OPS\t\t:\t%u (ops/unit)\t*\n", num_ops);
                printf("*\tWORKLOAD\t:\t%u (us)\t\t\t*\n", WORKLOAD);
	}

        stack<uint32_t> myStack;

        BCL::barrier();

	end = 0;
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

        BCL::barrier();

        cpu_time_used = ((double) end) / CLOCKS_PER_SEC;
	total_time = BCL::reduce(cpu_time_used, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
                printf("*\tTHROUGHPUT\t:\t%f (ops/s)\t*\n", ELEM_PER_UNIT / total_time);
                printf("*********************************************************\n");
	}

        //BCL::barrier();
        //printf("[%lu]cpu_time_used = %f s, failure = %lu\n", BCL::rank(), cpu_time_used, failure);

	BCL::finalize();

	return 0;
}
