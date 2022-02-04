#ifndef MEMORY_IBR_H
#define MEMORY_IBR_H

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <limits>	// std::numeric_limits...

namespace dds
{

namespace ibr
{

template<typename T>
struct block
{
	uint32_t	era_new;
	T		elem;
};

template<typename T>
struct sblock
{
	uint32_t	era_new;
	uint32_t	era_del;
	gptr<block<T>>	ptr;
};

struct reser
{
	uint32_t	upper;
	uint32_t	lower;
};

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
	bool try_reserve(gptr<T>&,		// try to protect a global pointer from reclamation
			const gptr<gptr<T>>&);
	void unreserve(const gptr<T>&);		// stop protecting a global pointer

private:
	const uint32_t		MIN		= std::numeric_limits<uint32_t>::min();
	const uint64_t		EPOCH_FREQ	= BCL::nprocs();	// freq. of increasing epoch
	const uint64_t		EMPTY_FREQ	= BCL::nprocs() * 2;	// freq. of reclaiming retired

	gptr<block<T>>			pool;		// allocate PGAS
	gptr<block<T>>			pool_rep;	// deallocate PGAS
	uint64_t			capacity;	// contain global memory capacity (bytes)
	gptr<uint32_t>			epoch;		// a global clock
	gptr<reser>			reservation;	// contain the reserved things
	uint64_t			counter;	// a local counter
	std::vector<sblock<T>>		list_ret;	// contain retired elems
	std::vector<gptr<block<T>>>	list_rec;	// contain reclaimed elems

	void empty();
};

} /* namespace ibr */

} /* namespace dds */

template<typename T>
dds::ibr::memory<T>::memory()
{
	epoch = BCL::alloc<uint32_t>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		mem_manager = "IBR";
		BCL::store(uint32_t(1), epoch);
	}
	else // if (BCL::rank() != MASTER_UNIT)
		epoch.rank = MASTER_UNIT;

	gptr<reser> reservation = BCL::alloc<reser>(1);
	BCL::store({MIN, MIN}, reservation);

	pool = pool_rep = BCL::alloc<block<T>>(TOTAL_OPS);
	capacity = pool.ptr + TOTAL_OPS * sizeof(block<T>);

	counter = 0;
	list_ret.reserve(EMPTY_FREQ);
}

template<typename T>
dds::ibr::memory<T>::~memory()
{
	BCL::dealloc<block<T>>(pool_rep);
	BCL::dealloc<reser>(reservation);
	epoch.rank = BCL::rank();
	BCL::dealloc<uint32_t>(epoch);
}

template<typename T>
dds::gptr<T> dds::ibr::memory<T>::malloc()
{
	++counter;
	if (counter % EPOCH_FREQ == 0)
		BCL::fao_sync(epoch, uint32_t(1), BCL::plus<uint32_t>{});	// one RMA

	// determine the global address of the new element
	if (!list_rec.empty())
	{
		// tracing
		#ifdef  TRACING
			elem_ru++;
		#endif

		gptr<block<T>> addr = list_rec.back();
		list_rec.pop_back();
		gptr<uint32_t> temp = {addr.rank, addr.ptr};
		uint32_t timestamp = aget_sync(epoch);	// one RMA
		BCL::rput_sync(timestamp, temp);	// one RMA
		return {addr.rank, addr.ptr + sizeof(addr.rank)};
	}
	else // the list of reclaimed global empty is empty
	{
		if (pool.ptr < capacity)
		{
			gptr<T> addr = {pool.rank, pool.ptr + sizeof(pool.rank)};
			gptr<uint32_t> temp = {pool.rank, pool.ptr};
			uint32_t timestamp = aget_sync(epoch);	// one RMA
			BCL::store(timestamp, temp);
			++pool;
			return addr;
		}
		else // if (pool.ptr == capacity)
		{
			empty();
			if (!list_rec.empty())
			{
				// tracing
				#ifdef  TRACING
					elem_ru++;
				#endif

				gptr<block<T>> addr = list_rec.back();
				list_rec.pop_back();
				gptr<uint32_t> temp = {addr.rank, addr.ptr};
				uint32_t timestamp = aget_sync(epoch);	// one RMA
				BCL::rput_sync(timestamp, temp);	// one RMA
				return {addr.rank, addr.ptr + sizeof(addr.rank)};
			}
		}
	}
	return nullptr;
}

template<typename T>
void dds::ibr::memory<T>::free(const gptr<T>& ptr)
{
	gptr<block<T>> temp = {ptr.rank, ptr.ptr - sizeof(ptr.rank)};
	gptr<uint32_t> temp2 = {temp.rank, temp.ptr};
	uint32_t era_new = BCL::rget_sync(temp2);	// one RMA
	uint32_t era_del = BCL::aget_sync(epoch);	// one RMA
	list_ret.push_back({era_new, era_del, temp});
	if (list_ret.size() % EMPTY_FREQ == 0)
		empty();
}

template<typename T>
void dds::ibr::memory<T>::op_begin()
{
	uint32_t timestamp = BCL::aget_sync(epoch);	// one RMA
	BCL::aput_sync({timestamp, timestamp}, reservation);
}

template<typename T>
void dds::ibr::memory<T>::op_end()
{
	BCL::aput_sync({MIN, MIN}, reservation);
}

template<typename T>
bool dds::ibr::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	uint32_t 	timestamp, timestamp2;
	gptr<uint32_t>	upper = {reservation.rank, reservation.ptr};
	while (true)
	{
		timestamp = aget_sync(epoch);	// one RMA
		if (aget_sync(upper) < timestamp)
			aput_sync(timestamp, upper);
		timestamp2 = aget_sync(epoch);   // one RMA
		if (aget_sync(upper) == timestamp2)
			return true;
		ptr = aget_sync(atom);	// one RMA
	}
}

template<typename T>
void dds::ibr::memory<T>::unreserve(const gptr<T>& ptr)
{
	/* No-op */
}

template<typename T>
void dds::ibr::memory<T>::empty()
{
	std::vector<reser>	reservations;
	gptr<reser>		temp = reservation;
	reser			value;
	reservations.reserve(BCL::nprocs());
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		temp.rank = i;
		value = BCL::aget_sync(temp);	// one RMA
		reservations.push_back(value);
	}

	bool conflict = false;
	for (uint64_t i = 0; i < list_ret.size(); ++i)
	{
		for (uint64_t j = 0; j < reservations.size(); ++j)
			if (list_ret[i].era_new <= reservations[j].upper &&
				list_ret[i].era_del >= reservations[j].lower)
					conflict = true;
		if (!conflict)
		{
			// tracing
			#ifdef	TRACING
				++elem_rc;
			#endif

			list_rec.push_back(list_ret[i].ptr);		
			list_ret.erase(list_ret.begin() + i);
		}
	}
}

#endif /* MEMORY_IBR_H */
