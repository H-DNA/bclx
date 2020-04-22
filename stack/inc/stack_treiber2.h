#ifndef STACK_TREIBER2_H
#define STACK_TREIBER2_H

#include <unistd.h>
#include "../../lib/backoff.h"

namespace dds
{

namespace ts2
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
        	const gptr<elem<T>> 	NULL_PTR 	= 	nullptr; 	//is a null constant
		const uint8_t		MAX_FAILURES 	= 	5;		//max CAS failures

		memory<elem<T>>		mem;	//handles global memory
                gptr<gptr<elem<T>>>	top;	//points to global address of the top
		gptr<uint8_t>		sta;	//state
		gptr<elem<T>>		tun;	//tunnel	
		uint8_t			peer;
	};

} /* namespace ts2 */

} /* namespace dds */

template<typename T>
dds::ts2::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR, top);
		printf("*\tSTACK\t\t:\tTS2\t\t\t*\n");
	}
	else
		top.rank = MASTER_UNIT;

	sta = BCL::alloc<uint8_t>(1);
	BCL::store((uint8_t) 0, sta);
	tun = BCL::alloc<elem<T>>(1);

	peer = (BCL::rank() + 1) % BCL::nprocs();

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::ts2::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<uint8_t>(sta);
	BCL::dealloc<elem<T>>(tun);
	BCL::dealloc<gptr<elem<T>>>(top);
}

template<typename T>
void dds::ts2::stack<T>::push(const T &value)
{
        gptr<elem<T>> 		oldTopAddr,
				newTopAddr;
	backoff::backoff	bk;
	uint8_t			numFailures = 0;

	//allocate global memory to the new elem
	newTopAddr = mem.malloc();
	if (newTopAddr == nullptr)
		return;

	while (true)
	{
		//get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		//update new element (global memory)
                BCL::rput_sync({oldTopAddr, value}, newTopAddr);

		//update top (global memory)
		if (BCL::cas_sync(top, oldTopAddr, newTopAddr) == oldTopAddr)
		{
			//if (BCL::rank() == MASTER_UNIT)
			//{
				gptr<uint8_t>	peerStaAddr;
				uint8_t		peerStaVal;	
				gptr<elem<T>>	peerTunAddr;
				elem<T>		peerTunVal;	

				while (true)
				{
					printf("[%lu]CAC\n", BCL::rank());

					//help someone
					peerStaAddr = {peer, sta.ptr};
					peerTunAddr = {peer, tun.ptr};
					peerStaVal = BCL::cas_sync(peerStaAddr, (uint8_t) 1, (uint8_t) 0);
					if (peerStaVal == 1)	//this peer needs help
					{
						//help it
                				oldTopAddr = BCL::aget_sync(top);
						peerTunVal = BCL::rget_sync(peerTunAddr);
                				BCL::rput_sync({oldTopAddr, peerTunVal.value}, peerTunVal.next);
                				if (BCL::cas_sync(top, oldTopAddr, peerTunVal.next) == oldTopAddr)
						{
							BCL::aput_sync((uint8_t) 2, peerStaAddr);	//help succeeds
							return;
						}
						else
						{
							BCL::aput_sync((uint8_t) 3, peerStaAddr);	//help fails

							//next peer
							peer = (peer + 1) / BCL::nprocs();
							if (peer == BCL::rank())
							{
								peer = (peer + 1) / BCL::nprocs();
								return;
							}
						}
					}
					else	//this peer needs no help
					{
						//next peer
                                                peer = (peer + 1) / BCL::nprocs();
						if (peer == BCL::rank())
						{
							peer = (peer + 1) / BCL::nprocs();
							return;
						}
					}
				}
			//}

			return;
		}
		else //if (BCL::cas_sync(top, oldTopAddr, newTopAddr) != oldTopAddr)
		{
			//debugging
			++failure;

			if (++numFailures > MAX_FAILURES)
			{
				BCL::store({newTopAddr, value}, tun);
				BCL::aput_sync((uint8_t) 1, sta);
				bk.delay_dbl();
				if (BCL::cas_sync(sta, (uint8_t) 1, (uint8_t) 0) != 1)	//some process is trying to help me
				{
					printf("CHO!\n");

					uint8_t	dang;
					do {
						dang = BCL::aget_sync(sta);
					} while (dang == 0);
					if (dang == 2)
						return;
				}
			}
			else {}
				bk.delay_dbl();
		}
	}
}

template<typename T>
bool dds::ts2::stack<T>::pop(T *value)
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
			#ifdef MEM_REC
        			//update hazard pointers
        			BCL::aput_sync(NULL_PTR, mem.hp);
			#endif

			value = NULL;
			return EMPTY;
		}

		#ifdef MEM_REC
			//update hazard pointers
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
			++failure;

			bk.delay_exp();
		}
	}

	#ifdef MEM_REC
		//update hazard pointers
		BCL::aput_sync(NULL_PTR, mem.hp);
	#endif

	//return the value of the popped elem
	*value = oldTopVal.value;

	//deallocate global memory of the popped elem
	mem.free(oldTopAddr);

	return NON_EMPTY;
}

template<typename T>
void dds::ts2::stack<T>::print()
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

#endif /* STACK_TREIBER2_H */

