#ifndef QUEUE_BLOCKING_H
#define QUEUE_BLOCKING_H

#ifdef MEM_REC
        #include "../../memory/memory_dang2.h"
#else
        #include "../../memory/memory_dang3.h"
#endif
#include "../../lock/lock_mcs.h"

namespace dds
{

namespace bq
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
		gptr<elem<T>>	next;
		T		value;
	};

	template <typename T>
	class queue
	{
	public:
		queue();			//collective
		~queue();			//collective
		void enqueue(const T &);	//non-collective
		bool dequeue(T *);		//non-collective
		void print();			//collective

	private:
		const gptr<elem<T>>	NULL_PTR = nullptr;	//is a null constant

		memory<elem<T>>		mem;
		mcsl::lock		lock;	//lock mutexs
		gptr<gptr<elem<T>>>	front;
		gptr<gptr<elem<T>>>	rear;
	};

} /* namespace bq */

} /* namespace dds */

template <typename T>
dds::bq::queue<T>::queue()
{
	//synchronize
	BCL::barrier();

        front = BCL::alloc<gptr<elem<T>>>(1);
	rear = BCL::alloc<gptr<elem<T>>>(1);
        if (BCL::rank() == MASTER_UNIT)
        {
                BCL::store(NULL_PTR, front);
		BCL::store(NULL_PTR, rear);
                printf("*\tQUEUE\t\t:\tBQ\t\t\t*\n");
        }
        else //if (BCL::rank() != MASTER_UNIT)
                front.rank = rear.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template <typename T>
dds::bq::queue<T>::~queue()
{
	if (BCL::rank() != MASTER_UNIT)
		front.rank = rear.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(front);
	BCL::dealloc<gptr<elem<T>>>(rear);
}

template <typename T>
void dds::bq::queue<T>::enqueue(const T &value)
{
        gptr<elem<T>>   oldRearAddr,
                        newRearAddr;

        //allocate global memory to the new elem
        newRearAddr = mem.malloc();
        if (newRearAddr == nullptr)
                return;

        //synchronize
        lock.acquire();

        //get rear (from global memory to local memory)
        oldRearAddr = BCL::rget_sync(rear);

        //update new element (global memory)
        BCL::rput_sync({oldRearAddr, value}, newRearAddr);

        //update rear (global memory)
        BCL::rput_sync(newRearAddr, rear);

	//update front (global memory)
	if (oldRearAddr == nullptr)
		BCL::rput_sync(newRearAddr, front);

        //synchronize
        lock.release();
}

template <typename T>
bool dds::bq::queue<T>::dequeue(T *value)
{
        elem<T>         oldFrontVal;
        gptr<elem<T>>   oldFrontAddr;

        //synchronize
        lock.acquire();

        //get front (from global memory to local memory)
        oldFrontAddr = BCL::rget_sync(front);

        if (oldFrontAddr == nullptr)
        {
                //synchronize
                lock.release();

                value = NULL;   //optional
                return EMPTY;
        }

        //get node (from global memory to local memory)
        oldFrontVal = BCL::rget_sync(oldFrontAddr);

        //update front
        BCL::rput_sync(oldFrontVal.next, front);

	//update rear
	if (oldFrontVal.next == nullptr)
		BCL::rput_sync(NULL_PTR, rear);

        //synchronize
        lock.release();

        //return the value of the dequeued elem
        *value = oldFrontVal.value;

        //deallocate global memory of the popped elem
        mem.free(oldFrontAddr);

        return NON_EMPTY;
}

template <typename T>
void dds::bq::queue<T>::print()
{
        //synchronize
        BCL::barrier();

        if (BCL::rank() == MASTER_UNIT)
        {
                gptr<elem<T>>   topAddr;
                elem<T>         topVal;

                for (topAddr = BCL::load(front); topAddr != nullptr; topAddr = topVal.next)
                {
                        topVal = BCL::rget_sync(topAddr);
                        printf("value = %d\n", topVal.value);
                        topVal.next.print();
                }
        }

        //synchronize
        BCL::barrier();
}

#endif /* QUEUE_BLOCKING_H */
