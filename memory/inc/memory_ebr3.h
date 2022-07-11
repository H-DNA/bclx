#ifndef MEMORY_EBR3_H
#define MEMORY_EBR3_h

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <utility>	// std::move...
#include <iterator>	// std::back_inserter...

namespace dds
{

namespace ebr3
{

template<typename T>
class memory
{
public:
	memory();
	~memory();
	gptr<T> malloc();			// allocate global memory
	void free(const gptr<T>&);		// deallocate global memory
	void op_begin();			// indicate the beginning of a concurrent operation
	void op_end();				// indicate the end of a concurrent operation
	gptr<T> reserve(const gptr<gptr<T>>&);	// try to protect a global pointer from reclamation
	void unreserve(const gptr<T>&);		// stop protecting a global pointer

private:
	const uint64_t		EMPTY_FREQ	= BCL::nprocs() * 2;	// freq. of reclaiming retired

	gptr<T>			pool;		// allocate global memory
	gptr<T>			pool_rep;	// deallocate global memory
	uint64_t		capacity;	// contain global memory capacity (bytes)
	std::vector<uint64_t>	knowledge;	// knowledge about the other processes
	gptr<uint64_t>		reservation;	// a SWMR variable
	uint64_t		counter;	// a local counter
	std::vector<gptr<T>>	list_ret;	// contain retired elems
	std::vector<gptr<T>>	list_rec;	// contain reclaimed elems

	void empty();
};

} /* namespace ebr3 */

} /* namespace dds */

template<typename T>
dds::ebr3::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "EBR3";

	reservation = BCL::alloc<uint64_t>(1);
	BCL::store(uint64_t(0), reservation);

	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
	capacity = pool.ptr + TOTAL_OPS * sizeof(T);

	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
		knowledge.push_back(0);
	counter = 0;
}

template<typename T>
dds::ebr3::memory<T>::~memory()
{
	BCL::dealloc<T>(pool_rep);
	BCL::dealloc<uint64_t>(reservation);
}

template<typename T>
dds::gptr<T> dds::ebr3::memory<T>::malloc()
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
void dds::ebr3::memory<T>::free(const gptr<T>& ptr)
{
	list_ret.push_back(ptr);
	++counter;
}

template<typename T>
void dds::ebr3::memory<T>::op_begin()
{
	BCL::fao_sync(reservation, uint64_t(1), BCL::plus<uint64_t>{});
	if (counter % EMPTY_FREQ == 0)
		empty();
}

template<typename T>
void dds::ebr3::memory<T>::op_end()
{
	BCL::fao_sync(reservation, uint64_t(1), BCL::plus<uint64_t>{});
}

template<typename T>
dds::gptr<T> dds::ebr3::memory<T>::reserve(const gptr<gptr<T>>& ptr)
{
	return aget_sync(ptr);	// one RMA
}

template<typename T>
void dds::ebr3::memory<T>::unreserve(const gptr<T>& ptr)
{
	/* No-op */
}

template<typename T>
void dds::ebr3::memory<T>::empty()
{
	gptr<uint64_t>		temp = reservation;
	std::vector<uint64_t>	reservations;
	uint64_t		value;
	reservations.reserve(BCL::nprocs());
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		temp.rank = i;
		value = BCL::aget_sync(reservation);	// one RMA
		reservations.push_back(value);
	}

	bool conflict = false;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		if (reservations[i] % 2 != 0 && reservations[i] == knowledge[i])
			conflict = true;
		knowledge[i] = reservations[i];
	}

	if (!conflict)
	{
		#ifdef	TRACING
			elem_rc += list_ret.size();
		#endif

		std::move(list_ret.begin(), list_ret.end(), std::back_inserter(list_rec));
		list_ret.clear();
	}
}

#endif /* MEMORY_EBR3_H */
