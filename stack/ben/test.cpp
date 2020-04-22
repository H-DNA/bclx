#include <ctime>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"

using namespace BCL;
using namespace dds;
using namespace dds::ts;

int main()
{
	uint32_t	i,
			value;
	bool		state;

        init();

	stack<uint32_t> myStack;

	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		value = (rank() + 1) * 10 + i;
		myStack.push(value);
	}

	myStack.print();
	if (rank() == MASTER_UNIT)
		printf("----------------------------------------------------------\n");
	barrier();

	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		state = myStack.pop(&value);
		if (state == NON_EMPTY)
			printf("[%lu]%d\n", rank(), value);
                else //if (state == EMPTY)
			printf("[%lu]EMPTY\n", rank());
	}

	myStack.print();
	if (rank() == MASTER_UNIT)
               	printf("----------------------------------------------------------\n");
	barrier();

	for (i = 0; i < ELEM_PER_UNIT; ++i)
	{
		value = (rank() + 1) * 100 + i;
		myStack.push(value);
	}

	myStack.print();
	/**/

	finalize();

	return 0;
}
