#ifndef TS_H
#define TS_H

#include "hazard_pointer.hpp"

namespace dds
{

namespace ts
{
        /* Data types */
	template <typename T>
	class stack
	{
	public:
		stack();		//collective
		~stack();		//collective
		void push(T);		//non-collective
		bool pop(T *);		//non-collective
		void print();		//collective

	private:
        	const BCL::GlobalPtr<elem<T>> 			NULLptr = nullptr; 	//is a null constant
		dds::memory_hp<T>				mem;			//handle global memory
                BCL::GlobalPtr<BCL::GlobalPtr<elem<T>>>		top;			//point to global address of the top
	};

} /* namespace ts */

} /* namespace dds */

template<typename T>
dds::ts::stack<T>::stack()
{
        //initialize value of hazard pointers
        BCL::rput(NULLptr, mem.hp);

	if (BCL::rank() == MASTER_UNIT)
	{
        	BCL::GlobalPtr<elem<T>> topTemp;

		//allocate global memory to the top (held by the master unit) of the stack
		topTemp = mem.malloc();
		top.rank = topTemp.rank;
		top.ptr = topTemp.ptr;

                //initialize value of top (dummy node)
                BCL::rput(NULLptr, top);
	}

	//broadcast the global address of the stack's top from the master unit to the others
	top = BCL::broadcast(top, MASTER_UNIT);
}

template<typename T>
dds::ts::stack<T>::~stack()
{
	/*T				value;
	BCL::GlobalPtr<elem<T>> 	topAddr;

	while (true)
	{
		topAddr = BCL::rget(top);
		if (topAddr != nullptr)
			pop(&value);
		else //if (topAddr == nullptr)
			break;
	}
	/**/
}

template<typename T>
void dds::ts::stack<T>::push(T value)
{
	elem<T> newTopVal;
        BCL::GlobalPtr<elem<T>> oldTopAddr, newTopAddr, result;

	//allocate global memory to the new elem
	newTopAddr = mem.malloc();
	if (newTopAddr == nullptr)
	{
		printf("[%lu]ERROR: The stack is full now. The push is ineffective.\n", BCL::rank());
		return;
	}

	//update new element (local memory)
	newTopVal.value = value;

	do {
		//get top (from global memory to local memory)
		oldTopAddr = BCL::rget(top);

		//update new element (global memory)
		newTopVal.next = oldTopAddr;
                BCL::rput(newTopVal, newTopAddr);

		//update top (global memory)
		BCL::compare_and_swap(top, &oldTopAddr, &newTopAddr, &result);

	} while (oldTopAddr != result);
}

template<typename T>
bool dds::ts::stack<T>::pop(T *value)
{
	elem<T> oldTopVal;
	BCL::GlobalPtr<elem<T>> oldTopAddr, oldTopAddr2, result;

	while (true)
	{
		//get top (from global memory to local memory)
		oldTopAddr = BCL::rget(top);

		if (oldTopAddr == nullptr)
		{
        		//update hazard pointers
        		BCL::rput(NULLptr, mem.hp);

			value = NULL;
			return EMPTY;
		}
		else //stack is not empty
		{
			//update hazard pointers
			BCL::rput(oldTopAddr, mem.hp);
			oldTopAddr2 = BCL::rget(top);
			if (oldTopAddr != oldTopAddr2)
				continue;

                        //get node (from global memory to local memory)
                        oldTopVal = BCL::rget(oldTopAddr);

                        //update top
                        BCL::compare_and_swap(top, &oldTopAddr, &oldTopVal.next, &result);
			if (oldTopAddr == result)
				break;
		}
	}

	/*do {
		//get top (from global memory to local memory)
		oldTop = BCL::rget_atomic(top);        //TODO: get only oldTop.next instead

		if (oldTop.next == nullptr)
		{
			value = NULL;
			return EMPTY;
		}
		else //stack is not empty
		{
			//update hazard pointers
			BCL::rput_atomic(oldTop.next, mem.hp);
			newTop = BCL::rget_atomic(top);
			if (oldTop.next != newTop.next)
				continue;

			//get node (from global memory to local memory)
			newTop = BCL::rget(oldTop.next);

			//debugging
			printf("[%d]value = %d\n", BCL::rank(), newTop.value);

			//update top
			BCL::compare_and_swap(top, &oldTop.next, &newTop.next, &result);
		}

	} while (oldTop.next != result);*/

	//update hazard pointers
	BCL::rput(NULLptr, mem.hp);

	//return the value of the popped elem
	*value = oldTopVal.value;

	//deallocate global memory of the popped elem
	mem.free(oldTopAddr);

	return NON_EMPTY;
}

template<typename T>
void dds::ts::stack<T>::print()
{
	//synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
		BCL::GlobalPtr<elem<T>> topAddr = BCL::rget(top);
		elem<T>			topVal;

		while (topAddr != nullptr)
		{
			topVal = BCL::rget(topAddr);
                	printf("value = %d\n", topVal.value);
                	topVal.next.print();
			topAddr = topVal.next;
		}
	}

	//synchronize
	BCL::barrier();
}

#endif /* TS_H */
