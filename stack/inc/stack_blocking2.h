#ifndef STACK_BLOCKING2_H
#define STACK_BLOCKING2_H

#include "../../lock/lock_mcs.h"

namespace dds
{

namespace bs2
{

	/* Macros */
        #ifdef MEM_REC
                using namespace dang2;
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
		stack();			//collective
		~stack();			//collective
		void push(const T &);		//non-collective
		bool pop(T *);			//non-collective
		void print();			//collective

	private:
        	const gptr<elem<T>>	NULL_PTR = nullptr;	//is a null constant

		memory<elem<T>>		mem;	//handle global memory
		mcsl::lock		lock;	//lock mutexs
                gptr<gptr<elem<T>>>	top;	//point to global address of the top
	};

} /* namespace bs2 */

} /* namespace dds */

template<typename T>
dds::bs2::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR, top);
                printf("*\tSTACK\t\t:\tBS2\t\t\t*\n");
	}
	else //if (BCL::rank() != MASTER_UNIT)
		top.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::bs2::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);
}

template<typename T>
void dds::bs2::stack<T>::push(const T &value)
{
        gptr<elem<T>> 	oldTopAddr,
			newTopAddr;

	//allocate global memory to the new elem
	newTopAddr = mem.malloc();
	if (newTopAddr == nullptr)
		return;

	//synchronize
	lock.acquire();

	//get top (from global memory to local memory)
	oldTopAddr = BCL::fao_sync(top, newTopAddr, BCL::replace<uint64_t>{});

	//update new element (global memory)
	#ifdef MEM_REC
        	BCL::rput_sync({oldTopAddr, value}, newTopAddr);
	#else
                BCL::store({oldTopAddr, value}, newTopAddr);
	#endif

	//synchronize
	lock.release();
}

template<typename T>
bool dds::bs2::stack<T>::pop(T *value)
{
	elem<T> 	oldTopVal;
	gptr<elem<T>> 	oldTopAddr;

	//synchronize
	lock.acquire();

	//get top (from global memory to local memory)
	oldTopAddr = BCL::rget_sync(top);

	if (oldTopAddr == nullptr)
	{
		//synchronize
		lock.release();

		value = NULL;	//optional
		return EMPTY;
	}

        //get node (from global memory to local memory)
        oldTopVal = BCL::rget_sync(oldTopAddr);

        //update top
	BCL::rput_sync(oldTopVal.next, top);

	//synchronize
	lock.release();

	//return the value of the popped elem
	*value = oldTopVal.value;

	//deallocate global memory of the popped elem
	mem.free(oldTopAddr);

	return NON_EMPTY;
}

template<typename T>
void dds::bs2::stack<T>::print()
{
	//synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<elem<T>> 	topAddr;
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

#endif /* STACK_BLOCKING2_H */
