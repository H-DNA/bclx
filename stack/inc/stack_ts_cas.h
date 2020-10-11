#ifndef STACK_TS_CAS_H
#define STACK_TS_CAS_H

#include <unistd.h>
#include "../../lib/utility.h"

namespace dds
{

namespace tss_cas
{

	/* Macros */
	#ifdef MEM_REC
		using namespace hp;
	#else
		using namespace dang3;
	#endif

	/* Data types */
	struct timestamp
	{
                uint64_t  	start;
                uint64_t  	end;

		bool operator<(const timestamp &) const;
	};

	class time
	{
	public:
		const uint64_t		ULLI_MIN 	= 0;			//lower bound
		const uint64_t		ULLI_MAX 	= 18446744073709551615U;//upper bound
                const timestamp		TS_MIN 		= {ULLI_MIN, ULLI_MIN};	//min TS
                const timestamp		TS_MAX 		= {ULLI_MAX, ULLI_MAX};	//max TS

		time();
		~time();
		timestamp getNewTS();

	private:
		const uint32_t		DELAY 	 	= TSS_INTERVAL;		//microseconds

		gptr<uint64_t>		clock;					//logical clock of the unit
	};

	template <typename T>
	struct elem
	{
                gptr<elem<T>>	next;
		bool		taken;
		timestamp	ts;
		T		value;
	};

	template <typename T>
	class stack
	{
        public:
                stack();                	//collective
		stack(const uint64_t &num);     //collective
                ~stack();               	//collective
                bool push(const T &value);	//non-collective
                bool pop(T &value);          	//non-collective
                void print();           	//collective

        private:
        	const gptr<elem<T>>	NULL_PTR = nullptr;

                gptr<gptr<elem<T>>>	top;
		memory<elem<T>>		mem;
		time			tim;

		bool push_fill(const T &value);
		gptr<elem<T>> get_youngest(const gptr<elem<T>> &);
		bool remove(const gptr<elem<T>> &, const gptr<elem<T>> &, T *);
		bool try_rem(const timestamp &, bool &, T *);
	};

} /* namespace tss_cas */

} /* namespace dds */

bool dds::tss_cas::timestamp::operator<(const timestamp &ts) const
{
	return (this->end < ts.start);
}

dds::tss_cas::time::time()
{
        clock = BCL::alloc<uint64_t>(1);
	if (BCL::rank() == MASTER_UNIT)
        	BCL::store((uint64_t) 1, clock);
	else //if (BCL::rank() != MASTER_UNIT)
		clock.rank = MASTER_UNIT;
}

dds::tss_cas::time::~time()
{
	if (BCL::rank() != MASTER_UNIT)
		clock.rank = BCL::rank();
        BCL::dealloc<uint64_t>(clock);
}

dds::tss_cas::timestamp dds::tss_cas::time::getNewTS()
{
	timestamp 	ts;

	ts.start = BCL::aget_sync(clock);

	//delay
	usleep(DELAY);

	ts.end = BCL::aget_sync(clock);
	if (ts.start != ts.end)
	{
		--ts.end;
		return ts;
	}

	ts.end = ts.start + 1;
	if (BCL::cas_sync(clock, ts.start, ts.end) == ts.start)
	{
		ts.end = ts.start;
		return ts;
	}
        ts.end = BCL::aget_sync(clock) - 1;
	return ts;
}

template <typename T>
dds::tss_cas::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	BCL::store(NULL_PTR, top);

	if (BCL::rank() == MASTER_UNIT)
                stack_name = "TSS_cas";

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::tss_cas::stack<T>::stack(const uint64_t &num)
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	BCL::store(NULL_PTR, top);

	if (BCL::rank() == MASTER_UNIT)
	{
		stack_name = "TSS_cas";

		for (uint64_t i = 0; i < num; ++i)
			push_fill(i);
	}

        //synchronize
        BCL::barrier();
}

template <typename T>
dds::tss_cas::stack<T>::~stack()
{
        BCL::dealloc<gptr<elem<T>>>(top);
}

template <typename T>
bool dds::tss_cas::stack<T>::push(const T &value)
{
        timestamp		ts;
	gptr<gptr<elem<T>>>	addrTemp;
	gptr<timestamp>		addrTemp2;
	gptr<elem<T>>		oldTopAddr,
				newTopAddr;
	elem<T>			oldTopVal,
				newTopVal;

	//Line number 12
        oldTopAddr = BCL::aget_sync(top);

	//Line number 13
        newTopAddr = mem.malloc();
        if (newTopAddr == nullptr)
        {
                printf("The stack is FULL\n");
                return false;
        }
        newTopVal = {oldTopAddr, false, tim.TS_MAX, value};
	BCL::store(newTopVal, newTopAddr);
       	BCL::aput_sync(newTopAddr, top);

        //unlinking
        while (oldTopAddr != nullptr)
        {
                oldTopVal = BCL::aget_sync(oldTopAddr);
                if (oldTopVal.taken)
                        oldTopAddr = oldTopVal.next;
                else //if (!topVal.taken)
                        break;
        }
	addrTemp = {newTopAddr.rank, newTopAddr.ptr};
	BCL::aput_sync(oldTopAddr, addrTemp);

	//Line number 14
	ts = tim.getNewTS();

	//Line number 15
        addrTemp2 = {newTopAddr.rank, newTopAddr.ptr +
				sizeof(gptr<elem<T>>) + sizeof(uint64_t)};
	BCL::aput_sync(ts, addrTemp2);

	return true;
}

template <typename T>
bool dds::tss_cas::stack<T>::pop(T &value)
{
	//elimination
	timestamp startTime = tim.getNewTS();

	bool success, result = NON_EMPTY;

	do {
		success = try_rem(startTime, result, &value);
	} while (!success);

	return result;
}

template <typename T>
void dds::tss_cas::stack<T>::print()
{
	//synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<gptr<elem<T>>>	topTemp;
		gptr<elem<T>>		topAddr;
		elem<T>			topVal;

		topTemp.ptr = top.ptr;
		for (int i = 0; i < BCL::nprocs(); ++i)
		{
			topTemp.rank = i;
			for (topAddr = BCL::load(topTemp); topAddr != nullptr; topAddr = topVal.next)
			{
				topVal = BCL::rget_sync(topAddr);
				if (!topVal.taken)
				{
					printf("[%d]value = %d, ts = {%lu, %lu}\n",
							i, topVal.value, topVal.ts.start, topVal.ts.end);
					topVal.next.print();
				}
			}
		}
	}

	//synchronize
	BCL::barrier();
}

template <typename T>
bool dds::tss_cas::stack<T>::push_fill(const T &value)
{
	gptr<elem<T>>		oldTopAddr,
				newTopAddr;
	elem<T>			newTopVal;

	//Line number 12
        oldTopAddr = BCL::aget_sync(top);

	//Line number 13
        newTopAddr = mem.malloc();
        if (newTopAddr == nullptr)
        {
                printf("The stack is FULL\n");
                return false;
        }
        newTopVal = {oldTopAddr, false, tim.getNewTS(), value};
	BCL::store(newTopVal, newTopAddr);
       	BCL::aput_sync(newTopAddr, top);

	return true;
}

template <typename T>
dds::gptr<dds::tss_cas::elem<T>> dds::tss_cas::stack<T>::get_youngest(const gptr<elem<T>> &topAddr)
{
	gptr<elem<T>>	tempAddr;
	elem<T>		tempVal;

	for (tempAddr = {topAddr.rank, topAddr.ptr}; tempAddr != nullptr; tempAddr = tempVal.next)
	{
		tempVal = BCL::aget_sync(tempAddr);
		if (!tempVal.taken)
			return tempAddr;
	}
	return nullptr;
}

template <typename T>
bool dds::tss_cas::stack<T>::remove(const gptr<elem<T>> &topVal, const gptr<elem<T>> &youngestAddr, T *value)
{
	bool 			oldVal = false,
				newVal = true,
				res_b;
	gptr<bool>		youngestTemp;
        gptr<gptr<elem<T>>>     topTemp;
	gptr<elem<T>>		tempAddr;
	elem<T>			youngestVal;

	youngestTemp = {youngestAddr.rank, youngestAddr.ptr + sizeof(gptr<elem<T>>)};
	res_b = BCL::cas_sync(youngestTemp, oldVal, newVal);
	if (res_b == oldVal)
	{
		//unlinking
		topTemp = {youngestAddr.rank, top.ptr};
		BCL::cas_sync(topTemp, topVal, youngestAddr);
		//unlinks elems before @youngestAddr in the list
		if (topVal != youngestAddr)
		{
			topTemp = {topVal.rank, topVal.ptr};
			BCL::aput_sync(youngestAddr, topTemp);
		}
		//unlinks elems after @youngestAddr in the list
		tempAddr = BCL::aget_sync(youngestAddr).next;
        	while (tempAddr != nullptr)
        	{
                	youngestVal = BCL::aget_sync(tempAddr);
                	if (youngestVal.taken)
                        	tempAddr = youngestVal.next;
                	else //if (!youngestVal.taken)
                        	break;
        	}
        	topTemp = {youngestAddr.rank, youngestAddr.ptr};
        	BCL::aput_sync(tempAddr, topTemp);
		/**/

		youngestVal = BCL::aget_sync(youngestAddr);
		*value = youngestVal.value;
		return true;
	}

	value = nullptr;
	return false;
}

template <typename T>
bool dds::tss_cas::stack<T>::try_rem(const timestamp &startTime, bool &result, T *value)
{
	int 			i;
	timestamp		tsMax;
	gptr<gptr<elem<T>>>	topTemp;
	gptr<elem<T>>		topAddr,
				tempAddr,
				youngestAddr,
				empty[BCL::nprocs()];
	elem<T>			tempVal;

	youngestAddr = nullptr;
	tsMax = tim.TS_MIN;
	topTemp.ptr = top.ptr;

	for (i = 0; i < BCL::nprocs(); ++i)
	{
		topTemp.rank = i;
		topAddr = BCL::aget_sync(topTemp);
		tempAddr = get_youngest(topAddr);

		//emptiness check
		if (tempAddr == nullptr)
		{
			empty[i] = topAddr;
			continue;
		}

		tempVal = BCL::aget_sync(tempAddr);

		//elimination
		if (startTime < tempVal.ts)
			return remove(topAddr, tempAddr, value);

		if (tsMax < tempVal.ts)
		{
			youngestAddr = tempAddr;
			tsMax = tempVal.ts;
		}
	}

	//emptiness check
	if (youngestAddr == nullptr)
	{
		value = NULL;
		for (i = 0; i < BCL::nprocs(); ++i)
		{
                	topTemp.rank = i;
                	topAddr = BCL::aget_sync(topTemp);
			if (topAddr != empty[i])
				return false;
		}

		result = EMPTY;
		return true;
	}
	/**/

	return remove(topAddr, youngestAddr, value);
}

#endif /* STACK_TS_CAS_H */
