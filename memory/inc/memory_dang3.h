#ifndef MEMORY_DANG3_H
#define MEMORY_DANG3_H

#include <cstdint>			// uint32_t...
#include <vector>			// std::vector...
#include <algorithm>			// std::sort...
#include <utility>			// std::move...
#include "../queue/inc/queue_spsc.h"	// dds::queue_spsc...

namespace dds
{

namespace dang3
{

/* Macros */
using namespace bclx;

/* Datatypes */
template<typename T>
struct list_seq2
{
	sds::list<T>		contig;
	std::vector<gptr<T>>	ncontig;
};

template<typename T>
class memory
{
public:
	memory();
	~memory();
	gptr<T> malloc();				// allocate global memory
	void free(const gptr<T>&);			// deallocate global memory
	void retire(const gptr<T>&);			// retire a global pointer
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

	sds::list<T>						pool_mem;	// allocate global memory
	gptr<T>         					pool_rep;	// deallocate global memory
	gptr<gptr<T>>						reservation;	// be a reservation array of the calling unit
	std::vector<gptr<T>>					list_ret;	// contain retired elems
	list_seq2<T>						lheap;		// be per-unit heap
	std::vector<std::vector<gptr<T>>>			buffers;	// be local buffers
	std::vector<std::vector<dds::queue_spsc<gptr<T>>>>	queues;		// be SPSC queues

        void empty();
};

} /* namespace dang3 */

} /* namespace dds */

template<typename T>
dds::dang3::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG3";

        gptr<gptr<T>> temp = reservation = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
		bclx::store(NULL_PTR, temp);
		++temp;
	}

	pool_rep = BCL::alloc<T>(TOTAL_OPS);
	if (pool_rep == nullptr)
	{
		printf("[%lu]ERROR: memory.memory\n", BCL::rank());
		return;
	}
        pool_mem.set(pool_rep, TOTAL_OPS);

	list_ret.reserve(HP_WINDOW);

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		queues.push_back(std::vector<dds::queue_spsc<gptr<T>>>());
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
			queues[i].push_back(dds::queue_spsc<gptr<T>>(i, TOTAL_OPS / BCL::nprocs()));
	}

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		buffers.push_back(std::vector<gptr<T>>());
		buffers[i].reserve(HP_WINDOW);
	}
}

template<typename T>
dds::dang3::memory<T>::~memory()
{
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
			queues[i][j].clear();

        BCL::dealloc<T>(pool_rep);
	BCL::dealloc<gptr<T>>(reservation);
}

template<typename T>
bclx::gptr<T> dds::dang3::memory<T>::malloc()
{
	// if lheap.ncontig is not empty, return a gptr<T> from it
	if (!lheap.ncontig.empty())
	{
		// tracing
		#ifdef	TRACING
			++elem_ru;
		#endif

		gptr<T> ptr = lheap.ncontig.back();
		lheap.ncontig.pop_back();
		return ptr;
	}

	// if lheap.contig is not empty, return a gptr<T> from it
	if (!lheap.contig.empty())
		return lheap.contig.pop();

	// otherwise, scan all queues to get reclaimed elems if any
	for (uint64_t i = 0; i < queues[BCL::rank()].size(); ++i)
	{
		std::vector<gptr<T>> slist;
		if (queues[BCL::rank()][i].dequeue(slist))
			lheap.ncontig.insert(lheap.ncontig.end(), slist.begin(), slist.end());
	}
	
	// if lheap.ncontig is not empty, return a gptr<T> from it
	if (!lheap.ncontig.empty())
	{
		// tracing
		#ifdef	TRACING
			++elem_ru;
		#endif

		gptr<T> ptr = lheap.ncontig.back();
		lheap.ncontig.pop_back();
		return ptr;
	}

	// otherwise, get elems from the memory pool
	if (!pool_mem.empty())
	{
		gptr<T> ptr = pool_mem.pop(HP_WINDOW);
        	lheap.contig.set(ptr, HP_WINDOW);

                return lheap.contig.pop();
	}

	// try to scan all queues to get relaimed elems one more
	for (uint64_t i = 0; i < queues[BCL::rank()].size(); ++i)
	{
		std::vector<gptr<T>> slist;
		if (queues[BCL::rank()][i].dequeue(slist))
			lheap.ncontig.insert(lheap.ncontig.end(), slist.begin(), slist.end());
        }

        // if lheap.ncontig is not empty, return a gptr<T> from it
        if (!lheap.ncontig.empty())
        {
		// tracing
		#ifdef	TRACING
			++elem_ru;
		#endif

		gptr<T> ptr = lheap.ncontig.back();
		lheap.ncontig.pop_back();
                return ptr;
        }
	
	// otherwise, return nullptr
	printf("[%lu]ERROR: memory.malloc\n", BCL::rank());
	return nullptr;
}

template<typename T>
void dds::dang3::memory<T>::free(const gptr<T>& ptr)
{
	if (ptr.rank == BCL::rank())
		lheap.ncontig.push_back(ptr);
	else // if (ptr.rank != BCL::rank())
	{
		buffers[ptr.rank].push_back(ptr);
		if (buffers[ptr.rank].size() >= HP_WINDOW)
		{
			queues[ptr.rank][BCL::rank()].enqueue(buffers[ptr.rank]);
			buffers[ptr.rank].clear();
		}
	}
}

template<typename T>
void dds::dang3::memory<T>::retire(const gptr<T>& ptr)
{
	list_ret.push_back(ptr);
	if (list_ret.size() >= HP_WINDOW)
		empty();
}

template<typename T>
void dds::dang3::memory<T>::op_begin() {}

template<typename T>
void dds::dang3::memory<T>::op_end() {}

template<typename T>
bclx::gptr<T> dds::dang3::memory<T>::try_reserve(const gptr<gptr<T>>& ptr, const gptr<T>& val_old)
{
	if (val_old == nullptr)
		return nullptr;
	else // if (val_old != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (bclx::aget_sync(temp) == NULL_PTR)
			{
				bclx::aput_sync(val_old, temp);
				gptr<T> val_new = bclx::aget_sync(ptr);	// one RMA
				if (val_new == nullptr || val_new != val_old)
					bclx::aput_sync(NULL_PTR, temp);
				return val_new;
			}
			else // if (bclx::aget_sync(temp) != NULL_PTR)
				++temp;
		printf("[%lu]ERROR: memory.try_reserve\n", BCL::rank());
		return nullptr;
	}
}

template<typename T>
bclx::gptr<T> dds::dang3::memory<T>::reserve(const gptr<gptr<T>>& ptr)
{
	gptr<T> val_old = bclx::aget_sync(ptr);	// one RMA
	if (val_old == nullptr)
		return nullptr;
	else // if (val_old != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (bclx::aget_sync(temp) == NULL_PTR)
			{
				gptr<T> val_new;
				while (true)
				{
					bclx::aput_sync(val_old, temp);
					val_new = bclx::aget_sync(ptr);	// one RMA
					if (val_new == nullptr)
					{
						bclx::aput_sync(NULL_PTR, temp);
						return nullptr;
					}
					else if (val_new == val_old)
						return val_old;
					else // if (val_new != val_old)
						val_old = val_new;
				}
			}
			else // if (bclx::aget_sync(temp) != NULL_PTR)
				++temp;
		printf("[%lu]ERROR: memory.reserve\n", BCL::rank());
		return nullptr;
	}
}

template<typename T>
void dds::dang3::memory<T>::unreserve(const gptr<T>& ptr)
{
	if (ptr == nullptr)
		return;
	else // if (ptr != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (bclx::aget_sync(temp) == ptr)
			{
				bclx::aput_sync(NULL_PTR, temp);
				return;
			}
			else // if (bclx::aget_sync(temp) != ptr)
				++temp;
		printf("[%lu]ERROR: memory.unreserve\n", BCL::rank());
		return;
	}
}

template<typename T>
void dds::dang3::memory<T>::empty()
{	
	std::vector<gptr<T>>	plist;		// contain non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// be dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// a temporary variable
	gptr<T>			ptr;		// a temporary variable

	plist.reserve(HP_TOTAL);
	new_dlist.reserve(HP_TOTAL);

	// Stage 1
	hp_temp.ptr = reservation.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		hp_temp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			ptr = bclx::aget_sync(hp_temp);
			if (ptr != NULL_PTR)
				plist.push_back(ptr);
			++hp_temp;
		}
	}
	
	// Stage 2
	std::sort(plist.begin(), plist.end());
	plist.resize(std::unique(plist.begin(), plist.end()) - plist.begin());

	// Stage 3
        while (!list_ret.empty())
	{
		ptr = list_ret.back();
		list_ret.pop_back();
		if (std::binary_search(plist.begin(), plist.end(), ptr))
			new_dlist.push_back(ptr);
		else
		{
			// tracing
			#ifdef	TRACING
				++elem_rc;
			#endif

			free(ptr);
		}
	}

	// Stage 4
	list_ret = std::move(new_dlist);
}

#endif /* MEMORY_DANG3_H */
