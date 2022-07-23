#ifndef MEMORY_DANG7_H
#define MEMORY_DANG7_H

#include <cstdint>			// uint32_t...
#include <vector>			// std::vector...
#include <algorithm>			// std::sort...
#include <utility>			// std::move...
#include "../pool/inc/pool_ubd_spsc.h"	// dds::queue_ubd_spsc...

namespace dds
{

namespace dang7
{

template<typename T>
class block
{
	gptr<T>	ptr;
	T	elem;
};

template<typename T>
class memory
{
public:
	memory();
	~memory();
	gptr<T> malloc();				// allocate global memory
	void free(const gptr<T>&);			// deallocate global memory
	void op_begin();				// indicate the beginning of a concurrent operation
	void op_end();					// indicate the end of a concurrent operation
	gptr<T> try_reserve(const gptr<gptr<T>>&,	// try to to protect a global pointer from reclamation
			const gptr<T>&);
	gptr<T> reserve(const gptr<gptr<T>>&);		// try to protect a global pointer from reclamation
	void unreserve(const gptr<T>&);			// stop protecting a global pointer

private:
	const gptr<T>		NULL_PTR 	= nullptr;
        const uint32_t		HPS_PER_UNIT 	= 1;
        const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
        const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

	gptr<block<T>>         					pool;		// allocate global memory
	gptr<block<T>>         					pool_rep;	// deallocate global memory
	uint64_t						capacity;	// contain global memory capacity (bytes)
	gptr<gptr<T>>						reservation;	// be a reservation array of the calling unit
	std::vector<gptr<T>>					list_ret;	// contain retired elems
	std::vector<gptr<T>>					list_rec;	// contain reclaimed elems
	uint64_t						counter;	// be a counter
	std::vector<std::vector<gptr<T>>>			buffers;	// be local buffers
	std::vector<std::vector<dds::pool_ubd_spsc<gptr<T>>>>	queues;		// be SPSC unbounded queues

        void empty();
};

} /* namespace dang7 */

} /* namespace dds */

template<typename T>
dds::dang7::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG7";

        gptr<gptr<T>> temp = reservation = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
		BCL::store(NULL_PTR, temp);
		++temp;
	}

	pool = pool_rep = BCL::alloc<block<T>>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(block<T>);

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		queues.push_back(std::vector<dds::queue_ubd_spsc<gptr<T>>>());
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
			queues[i].push_back(dds::queue_ubd_spsc<gptr<T>>(i, TOTAL_OPS / BCL::nprocs()));
	}

	counter = 0;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		buffers.push_back(std::vector<gptr<T>>());
		if (i != BCL::rank())
			buffers[i].reserve(HP_WINDOW);
	}
}

template<typename T>
dds::dang7::memory<T>::~memory()
{
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
			queues[i][j].clear();

        BCL::dealloc<block<T>>(pool_rep);
	BCL::dealloc<gptr<T>>(reservation);
}

template<typename T>
dds::gptr<T> dds::dang7::memory<T>::malloc()
{
	++counter;
	if (counter % HP_WINDOW == 0)
		for (uint64_t i = 0; i < queues[BCL::rank()].size(); ++i)
		{
			std::vector<gptr<T>> temp;
			if (queues[BCL::rank()][i].dequeue(temp))
			{
				//printf("[%lu]%lu\n", BCL::rank(), temp.size());
				list_rec.insert(list_rec.end(), temp.begin(), temp.end());
			}
		}

        // determine the global address of the new element
        if (!list_rec.empty())
	{
		// tracing
		#ifdef	TRACING
			++elem_ru;
		#endif

		gptr<T> addr = list_rec.back();
		list_rec.pop_back();
                return addr;
	}
        else // the list of reclaimed global memory is empty
        {
                if (pool.ptr < capacity)
                        return pool++;
                else // if (pool.ptr == capacity)
		{
			// try one more to reclaim global memory
			for (uint64_t i = 0; i < queues[BCL::rank()].size(); ++i)
			{
				std::vector<gptr<T>> temp;
                        	if (queues[BCL::rank()][i].dequeue(temp))
                                	list_rec.insert(list_rec.end(), temp.begin(), temp.end());
                       	}

			if (!list_rec.empty())
			{
				// tracing
				#ifdef  TRACING
					++elem_ru;
				#endif

				gptr<T> addr = list_rec.back();
				list_rec.pop_back();
				return addr;
			}
		}
        }
	return nullptr;
}

template<typename T>
void dds::dang7::memory<T>::free(const gptr<T>& addr)
{
	list_ret.push_back(addr);
	if (list_ret.size() >= HP_WINDOW)
		empty();
}

template<typename T>
void dds::dang7::memory<T>::op_begin()
{
	/* No-op */
}

template<typename T>
void dds::dang7::memory<T>::op_end()
{
	/* No-op */
}

template<typename T>
dds::gptr<T> dds::dang7::memory<T>::try_reserve(const gptr<gptr<T>>& ptr, const gptr<T>& val_old)
{
	if (val_old == nullptr)
		return nullptr;
	else // if (val_old != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (BCL::aget_sync(temp) == NULL_PTR)
			{
				BCL::aput_sync(val_old, temp);
				gptr<T> val_new = BCL::aget_sync(ptr);	// one RMA
				if (val_new == nullptr || val_new != val_old)
					BCL::aput_sync(NULL_PTR, temp);
				return val_new;
			}
			else // if (BCL::aget_sync(temp) != NULL_PTR)
				++temp;
		printf("HP:Error\n");
		return nullptr;
	}
}

template<typename T>
dds::gptr<T> dds::dang7::memory<T>::reserve(const gptr<gptr<T>>& ptr)
{
	gptr<T> val_old = BCL::aget_sync(ptr);	// one RMA
	if (val_old == nullptr)
		return nullptr;
	else // if (val_old != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (BCL::aget_sync(temp) == NULL_PTR)
			{
				gptr<T> val_new;
				while (true)
				{
					BCL::aput_sync(val_old, temp);
					val_new = BCL::aget_sync(ptr);	// one RMA
					if (val_new == nullptr)
					{
						BCL::aput_sync(NULL_PTR, temp);
						return nullptr;
					}
					else if (val_new == val_old)
						return val_old;
					else // if (val_new != val_old)
						val_old = val_new;
				}
			}
			else // if (BCL::aget_sync(temp) != NULL_PTR)
				++temp;
		printf("HP:Error\n");
		return nullptr;
	}
}

template<typename T>
void dds::dang7::memory<T>::unreserve(const gptr<T>& ptr)
{
	if (ptr == nullptr)
		return;
	else // if (ptr != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (BCL::aget_sync(temp) == ptr)
			{
				BCL::aput_sync(NULL_PTR, temp);
				return;
			}
			else // if (BCL::aget_sync(temp) != ptr)
				++temp;
		printf("HP:Error\n");
		return;
	}
}

template<typename T>
void dds::dang7::memory<T>::empty()
{	
	std::vector<gptr<T>>	plist;		// contain non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// be dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// a temporary variable
	gptr<T>			addr;		// a temporary variable

	plist.reserve(HP_TOTAL);
	new_dlist.reserve(HP_TOTAL);

	// Stage 1
	hp_temp.ptr = reservation.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		hp_temp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			addr = BCL::aget_sync(hp_temp);
			if (addr != NULL_PTR)
				plist.push_back(addr);
			++hp_temp;
		}
	}
	
	// Stage 2
	std::sort(plist.begin(), plist.end());
	plist.resize(std::unique(plist.begin(), plist.end()) - plist.begin());

	// Stage 3
        while (!list_ret.empty())
	{
		addr = list_ret.back();
		list_ret.pop_back();
		if (std::binary_search(plist.begin(), plist.end(), addr))
			new_dlist.push_back(addr);
		else
		{
			// tracing
			#ifdef	TRACING
				++elem_rc;
			#endif

			buffers[addr.rank].push_back(addr);
			if (buffers[addr.rank].size() == HP_WINDOW)
				queues[addr.rank][BCL::rank()].enqueue(std::move(buffers[addr.rank]));
		}
	}

	// Stage 4
	list_ret = std::move(new_dlist);
}

#endif /* MEMORY_DANG7_H */
