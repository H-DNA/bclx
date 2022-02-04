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
	gptr<T> malloc();			// allocate global memory
	void free(const gptr<T>&);		// deallocate global memory
	void op_begin();			// indicate the beginning of a concurrent operation
	void op_end();				// indicate the end of a concurrent operation
	bool try_reserve(gptr<T>&,		// try to protect a global pointer from reclamation
			const gptr<gptr<T>>&);
	void unreserve(const gptr<T>&);		// stop protecting a global pointer

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
	std::vector<gptr<T>>	list_rec;	// contain reclaimed elems

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
        	BCL::store(NULL_PTR, temp);
		++temp;
	}

        pool = pool_rep = BCL::alloc<block<T>>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(block<T>);
}

template<typename T>
dds::dang::memory<T>::~memory()
{
        BCL::dealloc<block<T>>(pool_rep);
	BCL::dealloc<gptr<T>>(reservation);
}

template<typename T>
dds::gptr<T> dds::dang::memory<T>::malloc()
{
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
		BCL::store(true, temp);
		list_all.push_back(addr);
		return addr;
	}
	else // the list of reclaimed global memory is empty
	{
                if (pool.ptr < capacity)
		{
			gptr<bool> temp = {pool.rank, pool.ptr};
			BCL::store(true, temp);
			gptr<T> addr = {pool.rank, pool.ptr + sizeof(pool.rank)};
			list_all.push_back(addr);
			pool++;
			return addr;
		}
                else // if (pool.ptr == capacity)
                {
                        // try one more to reclaim global memory
                        empty();
                        if (!list_rec.empty())
                        {
				// tracing
				#ifdef  TRACING
					++elem_ru;
				#endif

				gptr<T> addr = list_rec.back();
				list_rec.pop_back();			
				gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
                        	BCL::store(true, temp);
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
	gptr<bool> temp = {ptr.rank, ptr.ptr - sizeof(ptr.rank)};
	BCL::aput_sync(false, temp);

        if (list_all.size() >= HP_WINDOW)
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
bool dds::dang::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	gptr<gptr<T>> temp = reservation;
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
		if (BCL::aget_sync(temp) == NULL_PTR)
		{
			gptr<T> ptr_new;
			while (true)
			{
				BCL::aput_sync(ptr, temp);
				ptr_new = BCL::aget_sync(atom);
				if (ptr_new == nullptr)
				{
					BCL::aput_sync(NULL_PTR, temp);
					return false;
				}
				if (ptr_new == ptr)
					return true;
				ptr = ptr_new;
			}
		}
		else // if (BCL::aget_sync(temp) != NULL_PTR)
			++temp;
	printf("HP:Error\n");
	return false;
}

template<typename T>
void dds::dang::memory<T>::unreserve(const gptr<T>& ptr)
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
}

template<typename T>
void dds::dang::memory<T>::empty()
{
	std::vector<gptr<T>>	plist;		// contains non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// is dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// is a temporary variable
	gptr<T>			addr;		// is a temporary variable
	gptr<block<T>>		temp;		// is a temporary variable

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
			if (addr != nullptr)
				plist.push_back(addr);
			++hp_temp;
		}
	}
	
	// Stage 2
	std::sort(plist.begin(), plist.end());
	plist.resize(std::unique(plist.begin(), plist.end()) - plist.begin());

	// Stage 3
	temp.rank = BCL::rank();
        while (!list_all.empty())
	{
		addr = list_all.back();
		list_all.pop_back();
		temp.ptr = addr.ptr - sizeof(addr.rank);
		if (BCL::aget_sync(temp).taken || std::binary_search(plist.begin(), plist.end(), addr))
			new_dlist.push_back(addr);
		else
		{
			// tracing
			#ifdef	TRACING
				++elem_rc;
			#endif

			list_rec.push_back(addr);
		}
	}

	// Stage 4
	list_all = std::move(new_dlist);
}

#endif /* MEMORY_DANG_H */
