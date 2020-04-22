#ifndef QUEUE_MS_H
#define QUEUE_MS_H

#ifdef MEM_REC
        #include "../../memory/memory_hp.h"
#else
        #include "../../memory/memory_dang3.h"
#endif

namespace dds
{

namespace msq
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
		gptr<elem<T>>	next;
		T		value;
	};

	template <typename T>
	class queue
	{
	public:
		queue();			//collective
		~queue();			//collective
		bool enqueue(const T &);	//non-collective
		bool dequeue(T &);		//non-collective
		void print();			//collective

	private:
		const gptr<elem<T>>	NULL_PTR = nullptr;	//is a null constant

		memory<elem<T>>		mem;
		gptr<gptr<elem<T>>>	front;
		gptr<gptr<elem<T>>>	rear;
	};

} /* namespace msq */

} /* namespace dds */

template <typename T>
dds::msq::queue<T>::queue()
{
	//synchronize
	BCL::barrier();

        front = BCL::alloc<gptr<elem<T>>>(1);
	rear = BCL::alloc<gptr<elem<T>>>(1);
	gptr<elem<T>>> dummy = BCL::alloc<elem<T>>(1);

        if (BCL::rank() == MASTER_UNIT)
        {
		gptr<gptr<elem<T>>> temp = {dummmy.rank, dummy.ptr};
		BCL::store(NULL_PTR, temp};
                BCL::store(dummy, front);
		BCL::store(dummy, rear);
                printf("*\tQUEUE\t\t:\tMSQ\t\t\t*\n");
        }
        else //if (BCL::rank() != MASTER_UNIT)
                front.rank = rear.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template <typename T>
dds::msq::queue<T>::~queue()
{
	if (BCL::rank() != MASTER_UNIT)
		front.rank = rear.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(front);
	BCL::dealloc<gptr<elem<T>>>(rear);
}

template <typename T>
bool dds::msq::queue<T>::enqueue(const T &value)
{
        gptr<elem<T>>   	oldRearAddr,
                        	newRearAddr;
	elem<T>			oldRearVal;
	gptr<gptr<elem<T>>>	tempAddr;

        //allocate global memory to the new elem
        newRearAddr = mem.malloc();
        if (newRearAddr == nullptr)
                return false;

        //update new element (global memory)
        BCL::rput_sync({NULL_PTR, value}, newRearAddr);

	while (true)
	{
        	//get rear
        	oldRearAddr = BCL::aget_sync(rear);

		//get successor of rear
		oldRearVal = BCL::aget_sync(oldRearAddr);

		//are rear and its successor consistent?
		if (oldRearAddr == BCL::aget_sync(rear))
		{
			if (oldRearVal.next == nullptr)
			{
				tempAddr = {oldRearAddr.rank, oldRearAddr.ptr};
				if (BCL::cas_sync(tempAddr, oldRearVal.next, newRearAddr) == oldRearVal.next)
				       break;	
			}
			//else
			//	BCL::cas_sync(rear, oldRearAddr, oldRearVal.next);
		}
	}
	BCL::cas_sync(rear, oldRearAddr, newRearAddr);

	return true;
}

template <typename T>
bool dds::msq::queue<T>::dequeue(T &value)
{
        elem<T>         oldFrontVal;
        gptr<elem<T>>   oldFrontAddr,
			oldRearAddr;

	while (true)
	{
        	//get front
        	oldFrontAddr = BCL::aget_sync(front);

		//get rear
		oldRearAddr = BCL::aget_sync(rear);

		//get successor of front
		oldFrontVal = BCL::aget_sync(oldFrontAddr);

		//are front, its successor and rear consistent?
		if (oldFrontAddr == BCL::aget_sync(front))
		{
			if (oldFrontAddr == oldRearAddr)
			{
				if (oldFrontVal.next == nullptr)
					return false;
				BCL::cas_sync(rear, oldRearAddr, oldFrontVal.next);
			}
			else
			{
				//get value before CAS, otherwise another dequeue might free next node
				value = BCL::aget_sync(oldFrontVal.next).value;
				if (BCL::cas_sync(front, oldFrontAddr, oldFrontVal.next)
						break;
			}
		}
	}

        //deallocate global memory of the popped elem
        mem.free(oldFrontAddr);

        return true;
}

template <typename T>
void dds::msq::queue<T>::print()
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

#endif /* QUEUE_MS_H */
