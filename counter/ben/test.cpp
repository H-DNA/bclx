#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/counter.h"

using namespace dds;
using namespace dds::counter_nb2;

int main()
{
	uint64_t	i,
			value;
        clock_t         start,
                        end;
        double          cpu_time_used,
                        total_time;

	BCL::init();

        if (BCL::rank() == MASTER_UNIT)
        {
                printf("*********************************************************\n");
                printf("*\tBENCHMARK\t:\tTest\t\t\t*\n");
                printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
                printf("*\tNUM_OPS\t\t:\t%u (ops/unit)\t*\n", ELEM_PER_UNIT);
        }

	counter myCounter;

	BCL::barrier();

	start = clock();
	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
                //debugging
                #ifdef DEBUGGING
                        printf ("[%lu]%u\n", BCL::rank(), i);
                #endif

		value = myCounter.increment();
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
