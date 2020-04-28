#ifndef STACK_TREIBER_H
#define STACK_TREIBER_H

#include <unistd.h>
#include "../../lib/backoff.h"

namespace dds
{

namespace ts
{

	/* Macros */
	#ifdef MEM_REC
		using namespace hp;
	#else
		using namespace dang3;
	#endif

	/* Data types */
        template <typename T>
        struct elem
        {
                gptr<elem<T>>   next;
                T               value;
        };

	template <typename T>
	class stack
	{
	public:
		stack();		//collective
		~stack();		//collective
		void push(const T &);	//non-collective
		bool pop(T *);		//non-collective
		void print();		//collective

	private:
        	const gptr<elem<T>> 	NULL_PTR = nullptr; 	//is a null constant

		memory<elem<T>>		mem;	//handles global memory
                gptr<gptr<elem<T>>>	top;	//points to global address of the top
	};

} /* namespace ts */

} /* namespace dds */

template<typename T>
dds::ts::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR, top);
		printf("*\tSTACK\t\t:\tTS\t\t\t*\n");
	}
	else
		top.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::ts::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);
}

template<typename T>
void dds::ts::stack<T>::push(const T &value)
{
        gptr<elem<T>> 		oldTopAddr,
				newTopAddr;
	backoff::backoff	bk;		

	//allocate global memory to the new elem
	newTopAddr = mem.malloc();
	if (newTopAddr == nullptr)
		return;

	while (true)
	{
		//get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		//update new element (global memory)
		#ifdef MEM_REC
                	BCL::rput_sync({oldTopAddr, value}, newTopAddr);
		#else
                	BCL::store({oldTopAddr, value}, newTopAddr);
		#endif

		//update top (global memory)
		if (BCL::cas_sync(top, oldTopAddr, newTopAddr) == oldTopAddr)
			return;
		else //if (BCL::cas_sync(top, oldTopAddr, newTopAddr) != oldTopAddr)
		{
			//debugging
			#ifdef DEBUGGING
				++failure;
			#endif

			bk.delay_exp();
		}
	}
}

template<typename T>
bool dds::ts::stack<T>::pop(T *value)
{
	elem<T> 		oldTopVal;
	gptr<elem<T>> 		oldTopAddr,
				oldTopAddr2;
	backoff::backoff	bk;

	while (true)
	{
		//get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		if (oldTopAddr == nullptr)
		{
			//update hazard pointers
			#ifdef MEM_REC
        			BCL::aput_sync(NULL_PTR, mem.hp);
			#endif

			value = NULL;
			return EMPTY;
		}

		//update hazard pointers
		#ifdef MEM_REC
			BCL::aput_sync(oldTopAddr, mem.hp);
			oldTopAddr2 = BCL::aget_sync(top);
			if (oldTopAddr != oldTopAddr2)
				continue;
		#endif

		//get node (from global memory to local memory)
		oldTopVal = BCL::rget_sync(oldTopAddr);

		//update top
		if (BCL::cas_sync(top, oldTopAddr, oldTopVal.next) == oldTopAddr)
			break;
		else //if (BCL::cas_sync(top, oldTopAddr, oldTopVal.next) != oldTopAddr)
		{
			//debugging
			#ifdef DEBUGGING
				++failure;
			#endif

			bk.delay_exp();
		}
	}

	//update hazard pointers
	#ifdef MEM_REC
		BCL::aput_sync(NULL_PTR, mem.hp);
	#endif

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
		gptr<elem<T>>	topAddr;
		elem<T>		topVal;

		for (topAddr = BCL::load(top); topAddr != nullptr; topAddr = topVal.next)
		{
			topVal = BCL::rget_sync(topAddr);
                	printf("value = %d\n", topVal.value);
                	topVal.next.print();
		}
	}

	//synchronize
	BCL::barrier();
}

#endif /* STACK_TREIBER_H */

