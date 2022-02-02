#ifndef MEMORY_EBR_H
#define MEMORY_EBR_H

#include <cstdint>	// uint64_t...
#include <limits>	// std::numeric_limits...
#include <vector>	// std::vector...
#include <utility>	// std::move...
#include <iterator>	// std::back_inserter...

namespace dds
{

namespace ebr
{

	template<typename T>
	class memory
	{
	public:
		std::vector<gptr<T>>	list_rec;	// contain reclaimed elems

		memory();
		~memory();
		gptr<T> malloc();			// allocate global memory
		void free(const gptr<T>&);		// deallocate global memory
		void op_begin();			// indicate the beginning of a concurrent operation
		void op_end();				// indicate the end of a concurrent operation
		bool try_reserve(gptr<T>&,		// try to protect a global pointer from reclamation
				const gptr<gptr<T>>&);
		void unreserve(const gptr<T>&);		// stop protecting a global pointer

	private:
		const uint32_t		MIN		= std::numeric_limits<uint32_t>::min();
		const uint64_t		EPOCH_FREQ	= BCL::nprocs();	// freq. of increasing epoch

		gptr<T>			pool;		// allocate global memory
		gptr<T>			pool_rep;	// deallocate global memory
		uint64_t		capacity;	// contain global memory capacity (bytes)
		gptr<uint32_t>		epoch;		// a shared counter
		gptr<uint32_t>		reservation;	// a SWMR variable
		uint64_t		counter;	// a local counter
		uint32_t		curr;		// indicate the current epoch
		std::vector<gptr<T>>	list_ret[3];	// contain retired elems
	};

} /* namespace ebr */

} /* namespace dds */

template<typename T>
dds::ebr::memory<T>::memory()
{
	epoch = BCL::alloc<uint32_t>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		mem_manager = "EBR";
		BCL::store(uint32_t(1), epoch);
	}
	else // if (BCL::rank() != MASTER_UNIT)
		epoch.rank = MASTER_UNIT;

	reservation = BCL::alloc<uint32_t>(1);
	BCL::store(MIN, reservation);

	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
	capacity = pool.ptr + TOTAL_OPS * sizeof(T);

	counter = 0;
	curr = MIN;
	for (uint32_t i = 0; i < 3; ++i)
		list_ret[i].reserve(EPOCH_FREQ);
}

template<typename T>
dds::ebr::memory<T>::~memory()
{
	BCL::dealloc<T>(pool_rep);
	BCL::dealloc<uint32_t>(reservation);
	epoch.rank = BCL::rank();
	BCL::dealloc<uint32_t>(epoch);
}

template<typename T>
dds::gptr<T> dds::ebr::memory<T>::malloc()
{
	// determine the global address of the new element
	if (!list_rec.empty())
	{
		// tracing
		#ifdef	TRACING
			elem_ru++;
		#endif

		gptr<T> addr = list_rec.back();
		list_rec.pop_back();
		return addr;
	}
	else // the list of reclaimed global empty is empty
	{
		if (pool.ptr < capacity)
			return pool++;
		else // if (pool.ptr == capacity)
		{
			if (!list_rec.empty())
			{
				// tracing
				#ifdef	TRACING
					elem_ru++;
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
void dds::ebr::memory<T>::free(const gptr<T>& ptr)
{
	list_ret[curr-1].push_back(ptr);
}

template<typename T>
void dds::ebr::memory<T>::op_begin()
{
	uint32_t timestamp = BCL::aget_sync(epoch);	// one RMA

	if (curr != timestamp)
	{
		counter = 0;
		curr = timestamp;

		BCL::aput_sync(timestamp, reservation);
	}
	else // if (curr == timestamp)
	{
		++counter;
		if (counter % EPOCH_FREQ == 0)
		{
			uint32_t	value;
			gptr<uint32_t> 	addr = reservation;
			bool 		seen = true;
			for (uint64_t i = 0; i < BCL::nprocs(); ++i)
			{
				addr.rank = i;
				value = BCL::aget_sync(addr);	// one RMA
				if (value != MIN && value != timestamp)
				{
					seen = false;
					break;
				}
			}

			if (seen)
			{
				uint32_t index = curr % 3;

				// tracing
				#ifdef	TRACING
					elem_rc += list_ret[index].size();
				#endif

				std::move(list_ret[index].begin(), list_ret[index].end(),
						std::back_inserter(list_rec));
				list_ret[index].clear();

				curr = curr % 3 + 1;
				BCL::cas_sync(epoch, timestamp, curr);	// one RMA
			}
		}
		BCL::aput_sync(curr, reservation);
	}
}

template<typename T>
void dds::ebr::memory<T>::op_end()
{
	BCL::aput_sync(MIN, reservation);
}

template<typename T>
bool dds::ebr::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	return true;
}

template<typename T>
void dds::ebr::memory<T>::unreserve(const gptr<T>& ptr)
{
	/* No-op */
}

#endif /* MEMORY_EBR_H */
