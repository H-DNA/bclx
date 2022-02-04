#ifndef MEMORY_HE_H
#define MEMORY_HE_H

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <limits>	// std::numeric_limits...
#include <utility>	// std::pair...

namespace dds
{

namespace he
{

template<typename T>
struct block
{
	uint64_t	era_new;
	T		elem;
};

template<typename T>
struct sblock
{
	uint64_t	era_new;
	uint64_t	era_del;
	gptr<block<T>>	ptr;
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
	const uint64_t		MIN		= std::numeric_limits<uint64_t>::min();
	const uint32_t		HPS_PER_UNIT	= 1;
	const uint64_t		EPOCH_FREQ	= BCL::nprocs();	// freq. of increasing epoch
	const uint64_t		EMPTY_FREQ	= BCL::nprocs() * 2;	// freq. of reclaiming retired

	gptr<block<T>>					pool;		// allocate PGAS
	gptr<block<T>>					pool_rep;	// deallocate PGAS
	uint64_t					capacity;	// contain gmem capacity (bytes)
	gptr<uint64_t>					epoch;		// a global clock
	gptr<uint64_t>					reservation;	// contain the reserved things
	std::vector<std::pair<gptr<T>, uint32_t>>	dictionary;	// contain (ptr, index)s
	uint64_t					counter;	// a local counter
	std::vector<sblock<T>>				list_ret;	// contain retired elems
	std::vector<gptr<block<T>>>			list_rec;	// contain reclaimed elems

	void empty();
};

} /* namespace he */

} /* namespace dds */

template<typename T>
dds::he::memory<T>::memory()
{
	epoch = BCL::alloc<uint64_t>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		mem_manager = "HE";
		BCL::store(uint64_t(1), epoch);
	}
	else // if (BCL::rank() != MASTER_UNIT)
		epoch.rank = MASTER_UNIT;

	gptr<uint64_t> temp = reservation = BCL::alloc<uint64_t>(HPS_PER_UNIT);
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
		BCL::store(MIN, temp);
		++temp;
	}
	dictionary.reserve(HPS_PER_UNIT);

	pool = pool_rep = BCL::alloc<block<T>>(TOTAL_OPS);
	capacity = pool.ptr + TOTAL_OPS * sizeof(block<T>);

	counter = 0;
	list_ret.reserve(EMPTY_FREQ);
}

template<typename T>
dds::he::memory<T>::~memory()
{
	BCL::dealloc<block<T>>(pool_rep);
	BCL::dealloc<uint64_t>(reservation);
	epoch.rank = BCL::rank();
	BCL::dealloc<uint64_t>(epoch);
}

template<typename T>
dds::gptr<T> dds::he::memory<T>::malloc()
{
	// determine the global address of the new element
	if (!list_rec.empty())
	{
		// tracing
		#ifdef  TRACING
			elem_ru++;
		#endif

		gptr<block<T>> addr = list_rec.back();
		list_rec.pop_back();
		gptr<uint64_t> temp = {addr.rank, addr.ptr};
		uint64_t timestamp = aget_sync(epoch);	// one RMA
		BCL::rput_sync(timestamp, temp);	// one RMA
		return {addr.rank, addr.ptr + sizeof(uint64_t)};
	}
	else // the list of reclaimed global empty is empty
	{
		if (pool.ptr < capacity)
		{
			gptr<T> addr = {pool.rank, pool.ptr + sizeof(uint64_t)};
			gptr<uint64_t> temp = {pool.rank, pool.ptr};
			uint64_t timestamp = aget_sync(epoch);	// one RMA
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
				gptr<uint64_t> temp = {addr.rank, addr.ptr};
				uint64_t timestamp = aget_sync(epoch);	// one RMA
				BCL::rput_sync(timestamp, temp);	// one RMA
				return {addr.rank, addr.ptr + sizeof(uint64_t)};
			}
		}
	}
	return nullptr;
}

template<typename T>
void dds::he::memory<T>::free(const gptr<T>& ptr)
{
	uint64_t era_del = BCL::aget_sync(epoch);	// one RMA
	gptr<block<T>> temp = {ptr.rank, ptr.ptr - sizeof(uint64_t)};
	gptr<uint64_t> temp2 = {temp.rank, temp.ptr};
	uint64_t era_new = BCL::rget_sync(temp2);	// one RMA
	list_ret.push_back({era_new, era_del, temp});

	++counter;
	if (counter % EPOCH_FREQ == 0)
		BCL::cas_sync(epoch, era_del, era_del + 1);	// one RMA

	if (list_ret.size() % EMPTY_FREQ == 0)
		empty();
}

template<typename T>
void dds::he::memory<T>::op_begin()
{
	/* No-op */
}

template<typename T>
void dds::he::memory<T>::op_end()
{
	
}

template<typename T>
bool dds::he::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	uint64_t era_prev, era_curr;
	gptr<uint64_t> temp = reservation;
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
		if (BCL::aget_sync(temp) == MIN)
		{
			era_prev = BCL::aget_sync(epoch);	// one RMA
			BCL::aput_sync(era_prev, temp);
			while (true)
			{
				ptr = BCL::aget_sync(atom);	// one RMA
				era_curr = BCL::aget_sync(epoch);	// one RMA
				if (era_curr == era_prev)
				{
					dictionary.push_back(std::make_pair(ptr, i));
					return true;
				}
				BCL::aput_sync(era_curr, temp);
				era_prev = era_curr;
			}
		}
		else // if (era_prev != MIN)
			++temp;
	}
	printf("HE:Error\n");
	return false;
}

template<typename T>
void dds::he::memory<T>::unreserve(const gptr<T>& ptr)
{
	for (uint32_t i = 0; i < dictionary.size(); ++i)
		if (dictionary[i].first == ptr)
		{
			gptr<uint64_t> temp = reservation + dictionary[i].second;
			BCL::aput_sync(MIN, temp);
			return;
		}
}

template<typename T>
void dds::he::memory<T>::empty()
{
	bool			conflict;
	uint64_t		he;
	gptr<uint64_t> 		temp;
	std::vector<uint64_t>	reservations;

	reservations.reserve(BCL::nprocs() * HPS_PER_UNIT);
	temp.ptr = reservation.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		temp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			he = BCL::aget_sync(temp);
			if (he != MIN)
				reservations.push_back(he);
			++temp;
		}
	}

	for (uint64_t i = 0; i < list_ret.size(); ++i)
	{
		conflict = false;
		for (uint64_t j = 0; j < reservations.size(); ++j)
			if (list_ret[i].era_new <= reservations[j] &&
					reservations[j] <= list_ret[i].era_del)
			{
				conflict = true;
				break;
			}

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

#endif /* MEMORY_HE_H */
