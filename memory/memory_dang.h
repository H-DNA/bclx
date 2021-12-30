#ifndef MEMORY_DANG_H
#define MEMORY_DANG_H

#include <cstdint>	// uint_8, uint_32, uint64_t
#include <vector>	// std::vector
#include <algorithm>	// std::sort, std::unique, std::binary_search
#include <utility>	// std::move

namespace dds
{

namespace dang
{

	template<typename T>
	struct elem_dang
	{
		bool 	taken;
		T	elemD;
	};

	template<typename T>
	class memory
	{
	public:
		std::vector<gptr<T>>	list_recla;	// contain reclaimed elems

		memory();
		~memory();
                gptr<T> malloc();		// allocate global memory
                void free(const gptr<T>&);	// deallocate global memory
		void op_begin();		// indicate the beginning of a concurrent operation
		void op_end();			// indicate the end of a concurrent operation
		bool try_reserve(const gptr<T>&,// try to protect a global pointer from reclamation
				const gptr<gptr<T>>&);
		void unreserve(const gptr<T>&);	// stop protecting a global pointer

	private:
		const gptr<T>		NULL_PTR	= nullptr;
		const uint8_t		HPS_PER_UNIT	= 1;
		const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
		const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

                gptr<elem_dang<T>>	pool;           // allocate global memory
                gptr<elem_dang<T>>	pool_rep;    	// deallocate global memory
                uint64_t		capacity;       // contain global memory capacity (bytes)
		gptr<gptr<T>>		hp;		// be an array of hazard pointers
		std::vector<gptr<T>>	list_alloc;	// contain allocated elems

		void scan();
	};

} /* namespace dang */

} /* namespace dds */

template<typename T>
dds::dang::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG";

        hp = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
        BCL::store(NULL_PTR, hp);

        pool = pool_rep = BCL::alloc<elem_dang<T>>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(elem_dang<T>);

	list_alloc.reserve(HP_WINDOW);
	list_recla.reserve(HP_WINDOW);
}

template<typename T>
dds::dang::memory<T>::~memory()
{
        BCL::dealloc<elem_dang<T>>(pool_rep);
	BCL::dealloc<gptr<T>>(hp);
}

template<typename T>
dds::gptr<T> dds::dang::memory<T>::malloc()
{
        // determine the global address of the new element
	if (!list_recla.empty())
	{
		// tracing
		#ifdef  TRACING
			++elem_re;
		#endif

		gptr<T> addr = list_recla.back();
		list_recla.pop_back();
		gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
		BCL::store(true, temp);
		list_alloc.push_back(addr);
		return addr;
	}
	else // the list of reclaimed global memory is empty
	{
                if (pool.ptr < capacity)
		{
			gptr<bool> temp = {pool.rank, pool.ptr};
			BCL::store(true, temp);
			gptr<T> addr = {pool.rank, pool.ptr + sizeof(pool.rank)};
			list_alloc.push_back(addr);
			pool++;
			return addr;
		}
                else // if (pool.ptr == capacity)
                {
                        // try one more to reclaim global memory
                        scan();
                        if (!list_recla.empty())
                        {
				// tracing
				#ifdef  TRACING
					++elem_re;
				#endif

				gptr<T> addr = list_recla.back();
				list_recla.pop_back();			
				gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
                        	BCL::store(true, temp);
                        	list_alloc.push_back(addr);
				return addr;
			}
                        else // the list of reclaimed global memory is empty
				return nullptr;
                }
	}
}

template<typename T>
void dds::dang::memory<T>::free(const gptr<T>& addr)
{
	gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
	BCL::aput_sync(false, temp);

        if (list_alloc.size() >= HP_WINDOW)
                scan();
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
bool dds::dang::memory<T>::try_reserve(const gptr<T>& addr, const gptr<gptr<T>>& comp)
{
	gptr<gptr<T>> hp_temp = hp;
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
		if (BCL::aget_sync(hp_temp) == NULL_PTR)
		{
			BCL::aput_sync(addr, hp_temp);
			if (addr == BCL::aget_sync(comp))
				return true;
			return false;
		}
		else // if (BCL::aget_sync(hp_temp) != NULL_PTR)
			++hp_temp;
	return false;
}

template<typename T>
void dds::dang::memory<T>::unreserve(const gptr<T>& addr)
{
	gptr<gptr<T>> hp_temp = hp;
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
		if (BCL::aget_sync(hp_temp) == addr)
		{
			BCL::aget_sync(NULL_PTR, hp_temp);
			return;
		}
		else // if (BCL::aget_sync(hp_temp) != addr)
			++hp_temp;
}

template<typename T>
void dds::dang::memory<T>::scan()
{
	std::vector<gptr<T>>	plist;		// contains non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// is dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// Temporary variable
	gptr<T>			addr;		// Temporary variable
	gptr<elem_dang<T>>	temp;		// Temporary variable

	plist.reserve(HP_TOTAL);
	new_dlist.reserve(HP_TOTAL);

	// Stage 1
	hp_temp.ptr = hp.ptr;
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
        while (!list_alloc.empty())
	{
		addr = list_alloc.back();
		list_alloc.pop_back();
		temp.ptr = addr.ptr - sizeof(addr.rank);
		if (BCL::aget_sync(temp).taken || std::binary_search(plist.begin(), plist.end(), addr))
			new_dlist.push_back(addr);
		else
			list_recla.push_back(addr);
	}

	// Stage 4
	list_alloc = std::move(new_dlist);
}

#endif /* MEMORY_DANG_H */
