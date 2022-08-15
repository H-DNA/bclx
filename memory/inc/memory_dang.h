#ifndef MEMORY_DANG_H
#define MEMORY_DANG_H

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <algorithm>	// std::sort...
#include <utility>	// std::move...

namespace dds
{

namespace dang
{

using namespace bclx;

template<typename T>
struct block
{
	bool 	taken;
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
	gptr<T> try_reserve(const gptr<gptr<T>>&,	// try to protect a global pointer from reclamation
				const gptr<T>&);
	gptr<T> reserve(const gptr<gptr<T>>&);		// protect a global pointer from reclamation
	void unreserve(const gptr<T>&);			// stop protecting a global pointer

private:
	const gptr<T>		NULL_PTR	= nullptr;
	const uint8_t		HPS_PER_UNIT	= 1;
	const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
	const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

	gptr<block<T>>		pool;           // allocate global memory
	gptr<block<T>>		pool_rep;    	// deallocate global memory
	uint64_t		capacity;       // contain global memory capacity (bytes)
	gptr<gptr<T>>		reservation;	// be an array of hazard pointers
	std::vector<gptr<T>>	list_all;	// contain allocated elems
	std::vector<gptr<T>>	list_ret;	// contain retired elems
	std::vector<gptr<T>>	list_rec;	// contain reclaimed elems
	uint64_t		counter;	// be a counter

	void empty();
};

} /* namespace dang */

} /* namespace dds */

template<typename T>
dds::dang::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG";

        gptr<gptr<T>> temp = reservation = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
        	bclx::store(NULL_PTR, temp);
		++temp;
	}

        pool = pool_rep = BCL::alloc<block<T>>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(block<T>);

	counter = 0;
	list_ret.reserve(HP_WINDOW);
}

template<typename T>
dds::dang::memory<T>::~memory()
{
        BCL::dealloc<block<T>>(pool_rep);
	BCL::dealloc<gptr<T>>(reservation);
}

template<typename T>
bclx::gptr<T> dds::dang::memory<T>::malloc()
{
	++counter;
	if (counter % HP_WINDOW == 0)
	{
		gptr<bool> temp;
		for (uint64_t i = 0; i < list_all.size(); ++i)
		{
			temp = {list_all[i].rank, list_all[i].ptr - sizeof(list_all[i].rank)};
			if (!bclx::aget_sync(temp))
			{
				list_rec.push_back(list_all[i]);
				list_all.erase(list_all.begin() + i);
			}
		}
	}

        // determine the global address of the new element
	if (!list_rec.empty())
	{
		// tracing
		#ifdef  TRACING
			++elem_ru;
		#endif

		gptr<T> addr = list_rec.back();
		list_rec.pop_back();
		gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
		bclx::store(true, temp);
		list_all.push_back(addr);
		return addr;
	}
	else // the list of reclaimed global memory is empty
	{
                if (pool.ptr < capacity)
		{
			gptr<bool> temp = {pool.rank, pool.ptr};
			bclx::store(true, temp);
			gptr<T> addr = {pool.rank, pool.ptr + sizeof(pool.rank)};
			list_all.push_back(addr);
			pool++;
			return addr;
		}
                else // if (pool.ptr == capacity)
                {
                        // try one more to reclaim global memory
			gptr<bool> temp;
			for (uint64_t i = 0; i < list_all.size(); ++i)
			{
				temp = {list_all[i].rank, list_all[i].ptr - sizeof(list_all[i].rank)};
				if (!bclx::aget_sync(temp))
				{
					list_rec.push_back(list_all[i]);
					list_all.erase(list_all.begin() + i);
				}
			}

                        if (!list_rec.empty())
                        {
				// tracing
				#ifdef  TRACING
					++elem_ru;
				#endif

				gptr<T> addr = list_rec.back();
				list_rec.pop_back();			
				gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
                        	bclx::store(true, temp);
                        	list_all.push_back(addr);
				return addr;
			}
                }
	}
	return nullptr;
}

template<typename T>
void dds::dang::memory<T>::free(const gptr<T>& ptr)
{
	list_ret.push_back(ptr);
	if (list_ret.size() >= HP_WINDOW)
		empty();
}

template<typename T>
void dds::dang::memory<T>::op_begin()
{
	/* No-op */
}

template<typename T>
void dds::dang::memory<T>::op_end()
{
	/* No-op */
}

template<typename T>
bclx::gptr<T> dds::dang::memory<T>::try_reserve(const gptr<gptr<T>>& ptr, const gptr<T>& val_old)
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
		printf("HP:Error\n");
		return nullptr;
	}
}

template<typename T>
bclx::gptr<T> dds::dang::memory<T>::reserve(const gptr<gptr<T>>& ptr)
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
		printf("HP:Error\n");
		return nullptr;
	}
}

template<typename T>
void dds::dang::memory<T>::unreserve(const gptr<T>& ptr)
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
		printf("HP:Error\n");
		return;
	}
}

template<typename T>
void dds::dang::memory<T>::empty()
{
	std::vector<gptr<T>>	plist;		// contains non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// is dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// is a temporary variable
	gptr<T>			addr;		// is a temporary variable

	plist.reserve(HP_TOTAL);
	new_dlist.reserve(HP_TOTAL);

	// Stage 1
	hp_temp.ptr = reservation.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		hp_temp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			addr = bclx::aget_sync(hp_temp);
			if (addr != nullptr)
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

			// help another process reclaim a data element
			gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
			bclx::aput_sync(false, temp);
		}
	}

	// Stage 4
	list_ret = std::move(new_dlist);
}

#endif /* MEMORY_DANG_H */
