#ifndef MEMORY_EBR2_H
#define MEMORY_EBR2_H

#include <cstdint>	// uint64_t...
#include <limits>	// std::numeric_limits...
#include <vector>	// std::vector...
#include <algorithm>	// std::min_element...

namespace dds
{

namespace ebr2
{
	
	template<typename T>
	struct block
	{
		uint64_t	era_del;
		gptr<T>		ptr;
	};

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
		const uint64_t		MAX 		= std::numeric_limits<uint64_t>::max();
		const uint64_t		EPOCH_FREQ 	= BCL::nprocs();	// freq. of increasing epoch
		const uint64_t		EMPTY_FREQ 	= BCL::nprocs() * 2;	// freq. of reclaiming retired

		gptr<T>			pool;		// allocate global memory
		gptr<T>			pool_rep;	// deallocate global memory
		uint64_t		capacity;	// contain global memory capacity (bytes)
		gptr<uint64_t>		epoch;		// a shared counter
		gptr<uint64_t>		reservation;	// a SWMR variable
		uint64_t		counter;	// a local counter
		std::vector<block<T>>	list_ret;	// contain retired elems

		void empty();
	};
} /* namespace ebr2 */

} /* namespace dds */

template<typename T>
dds::ebr2::memory<T>::memory()
{
	epoch = BCL::alloc<uint64_t>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		mem_manager = "EBR2";
		BCL::store(uint64_t(0), epoch);
	}
	else // if (BCL::rank() != MASTER_UNIT)
		epoch.rank = MASTER_UNIT;

	reservation = BCL::alloc<uint64_t>(1);
	BCL::store(MAX, reservation);

	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
	capacity = pool.ptr + TOTAL_OPS * sizeof(T);

	counter = 0;
	list_ret.reserve(EMPTY_FREQ);
}

template<typename T>
dds::ebr2::memory<T>::~memory()
{
	BCL::dealloc<T>(pool_rep);
	BCL::dealloc<uint64_t>(reservation);
	epoch.rank = BCL::rank();
	BCL::dealloc<uint64_t>(epoch);
}

template<typename T>
dds::gptr<T> dds::ebr2::memory<T>::malloc()
{
	// determine the global address of the new element
	if (!list_rec.empty())
	{
		// tracing
		#ifdef	TRACING
			elem_re++;
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
			empty();
			if (!list_rec.empty())
			{
				// tracing
				#ifdef	TRACING
					elem_re++;
				#endif

				gptr<T> addr = list_rec.back();
				list_rec.pop_back();
				return addr;
			}
			else // the list of reclaimed global memory is empty
				return nullptr;
		}
	}
}

template<typename T>
void dds::ebr2::memory<T>::free(const gptr<T>& ptr)
{
	uint64_t timestamp = BCL::aget_sync(epoch);
	list_ret.push_back({timestamp, ptr});
	counter++;
	if (counter % EPOCH_FREQ == 0)
		BCL::fao_sync(epoch, uint64_t(1), BCL::plus<uint64_t>{});
	if (list_ret.size() % EMPTY_FREQ == 0)
		empty();
}

template<typename T>
void dds::ebr2::memory<T>::op_begin()
{
	uint64_t timestamp = BCL::aget_sync(epoch);
	BCL::aput_sync(timestamp, reservation);
}

template<typename T>
void dds::ebr2::memory<T>::op_end()
{
	BCL::aput_sync(MAX, reservation);
}

template<typename T>
bool dds::ebr2::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	return true;
}

template<typename T>
void dds::ebr2::memory<T>::unreserve(const gptr<T>& ptr)
{
	/* No-op */
}

template<typename T>
void dds::ebr2::memory<T>::empty()
{
	std::vector<uint64_t>	reservations;
	gptr<uint64_t>		temp = reservation;
	uint64_t		value;

	reservations.reserve(BCL::nprocs());

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		temp.rank = i;
		value = BCL::aget_sync(temp);
		reservations.push_back(value);
	}

	uint64_t max_safe_epoch = *std::min_element(reservations.begin(), reservations.end());

	for (uint64_t i = 0; i < list_ret.size(); ++i)
		if (list_ret[i].era_del < max_safe_epoch)
		{
			list_rec.push_back(list_ret[i].ptr);
			list_ret.erase(list_ret.begin() + i);
		}
}

#endif /* MEMORY_EBR2_H */
