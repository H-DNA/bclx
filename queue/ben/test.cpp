#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/queue.h"

using namespace BCL;
using namespace dds;
using namespace dds::bq;

int main()
{
	uint32_t 	i,
			value;
	bool		state;
	clock_t 	start,
			end;
	double		cpu_time_used,
			total_time;

        init();

        if (BCL::rank() == MASTER_UNIT)
        {
                printf("*********************************************************\n");
                printf("*\tBENCHMARK\t:\tTest\t\t\t*\n");
                printf("*\tNUM_UNITS\t:\t%lu\t\t\t*\n", BCL::nprocs());
                printf("*\tNUM_ELEMS\t:\t%u (elems/unit)\t\t*\n", ELEM_PER_UNIT);
        }

	if (rank() == MASTER_UNIT)
		printf("----------------------------------------------------------\n");
	barrier();

	queue<uint32_t> myQueue;

	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		value = (rank() + 1) * 10 + i;
		myQueue.enqueue(value);
	}

	myQueue.print();
	if (rank() == MASTER_UNIT)
		printf("----------------------------------------------------------\n");
	barrier();

	/*//start = clock();
	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		state = myQueue.dequeue(&value);
		if (state == NON_EMPTY)
			printf("[%lu]%d\n", rank(), value);
                else //if (state == EMPTY)
			printf("[%lu]EMPTY\n", rank());
	}
	//end = clock();
	//cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	//printf("cpu_time_used = %f microseconds\n", 1000000 * cpu_time_used);

	myQueue.print();
	if (rank() == MASTER_UNIT)
               	printf("----------------------------------------------------------\n");
	barrier();

	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		value = (rank() + 1) * 100 + i;
		myQueue.enqueue(value);
	}

	myQueue.print();
	/**/

        cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
        total_time = BCL::reduce(cpu_time_used, MASTER_UNIT, BCL::max<double>{});
        if (BCL::rank() == MASTER_UNIT)
        {
                printf("*\tEXEC_TIME\t:\t%f (s)\t\t*\n", total_time);
                printf("*\tTHROUGHPUT\t:\t%f (ops/ms)\t*\n", BCL::nprocs() * ELEM_PER_UNIT / total_time / 1000);
                printf("*********************************************************\n");
        }

	finalize();

	return 0;
}
