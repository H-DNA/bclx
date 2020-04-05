#include <ctime>
#include <bcl/bcl.hpp>
#include "./inc/queue.h"

using namespace pgas;
using namespace dds;
using namespace dds::bq;

int main(int argc, char **argv)
{
	int32_t i, value;
	bool state;
	clock_t start, end;
	double cpu_time_used;

        init();
        printf("[%lu]PROGRAM STARTS\n", my_uid());

	/* MAIN */
		queue<int32_t> myQueue;

		/*for (i = 0; i < ELEM_PER_UNIT; ++i)
		{
			value = (rank() + 1) * 10 + i;
			myQueue.enqueue(value);
		}

		myQueue.print();
		if (rank() == MASTER_UNIT)
			printf("----------------------------------------------------------\n");
		barrier();

		//start = clock();
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

        printf("[%lu]PROGRAM FINISHS\n", my_uid());
	finalize();

	return 0;
}
