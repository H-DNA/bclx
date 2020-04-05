#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"

using namespace dds;
using namespace dds::tss_cas;

int main()
{
        uint32_t 	i,
			value;
        bool 		state;
        clock_t 	start,
			end;
        double 		cpu_time_used,
			total_time;

        BCL::init();

	if (BCL::rank() == MASTER_UNIT)
	{
		printf("*********************************************************\n");
		printf("*\tBENCHMARK\t:\tProducer-consumer\t*\n");
		printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
		printf("*\tNUM_ELEMS\t:\t%u (elems/unit)\t*\n", ELEM_PER_UNIT);
	}

	if (BCL::nprocs() % 2 != 0)
	{
		printf("ERROR: The number of units must be even!\n");
		return -1;
	}

        stack<uint32_t> myStack;

        BCL::barrier();

        start = clock();
	if (BCL::rank() % 2 == 0)
		for (i = 0; i < ELEM_PER_UNIT; ++i)
		{
			//debugging
			#ifdef DEBUGGING
               			printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			value = i;
			myStack.push(value);
		}
	else //if (BCL::rank() % 2 != 0)
		for (i = 0; i < ELEM_PER_UNIT; ++i)
		{
                        //debugging
			#ifdef DEBUGGING
                        	printf ("[%lu]%u\n", BCL::rank(), i);
			#endif

			state = myStack.pop(&value);
		}
        end = clock();

        BCL::barrier();

        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	total_time = BCL::reduce(cpu_time_used, MASTER_UNIT, BCL::max<double>{});
	if (BCL::rank() == MASTER_UNIT)
	{
        	printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
		printf("*\tTHROUGHPUT\t:\t%f (ops/ms)\t*\n", BCL::nprocs() * ELEM_PER_UNIT / total_time / 1000);
                printf("*********************************************************\n");
	}

	BCL::finalize();

	return 0;
}
