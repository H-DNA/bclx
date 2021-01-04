#ifndef MEMORY_HP_H
#define MEMORY_HP_H

#include <cstdint>	// uint8_t, uint32_t, uint64_t
#include <vector>	// std::vector
#include <algorithm>	// std::sort, std::unique, std::binary_search
#include <utility>	// std::move

namespace dds
{

namespace hp
{
        template <typename T>
        class memory
        {
        public:
                gptr<gptr<T>> 	hp;		// Hazard pointer

                memory();
                ~memory();
		gptr<T> malloc();		// allocates global memory
                void free(const gptr<T>&);	// deallocates global memory

	private:
                const gptr<T>		NULL_PTR 	= nullptr;
        	const uint8_t		HPS_PER_UNIT 	= 1;
        	const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
        	const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

                gptr<T>         	pool;		// allocates global memory
                gptr<T>         	pool_rep;	// deallocates global memory
                uint64_t		capacity;	// contains global memory capacity (bytes)
                std::vector<gptr<T>>	list_delet;	// contains deleted elems
                std::vector<gptr<T>>	list_recla;	// contains reclaimed elems

                void scan();
        };

} /* namespace hp */

} /* namespace dds */

template <typename T>
dds::hp::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "HP";

        hp = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
        BCL::store(NULL_PTR, hp);

	pool = pool_rep = BCL::alloc<T>(ELEMS_PER_UNIT);
        capacity = pool.ptr + ELEMS_PER_UNIT * sizeof(T);

	list_recla.reserve(HP_WINDOW);
	list_delet.reserve(HP_WINDOW);
}

template <typename T>
dds::hp::memory<T>::~memory()
{
        BCL::dealloc<T>(pool_rep);
	BCL::dealloc<gptr<T>>(hp);
}

template <typename T>
dds::gptr<T> dds::hp::memory<T>::malloc()
{
        //determine the global address of the new element
        if (!list_recla.empty())
	{
		//tracing
		#ifdef	TRACING
			++elem_re;
		#endif

		gptr<T> addr = list_recla.back();
		list_recla.pop_back();
                return addr;
	}
        else //the list of reclaimed global memory is empty
        {
                if (pool.ptr < capacity)
                        return pool++;
                else //if (pool.ptr == capacity)
		{
			//try one more to reclaim global memory
			scan();
			if (!list_recla.empty())
			{
				//tracing
				#ifdef  TRACING
					++elem_re;
				#endif

				gptr<T> addr = list_recla.back();
				list_recla.pop_back();
				return addr;
			}
			else //the list of reclaimed global memory is empty
				return nullptr;
		}
        }
}

template <typename T>
void dds::hp::memory<T>::free(const gptr<T>& addr)
{
	list_delet.push_back(addr);
	if (list_delet.size() >= HP_WINDOW)
		scan();
}

template <typename T>
void dds::hp::memory<T>::scan()
{	
	std::vector<gptr<T>>	plist;		// contains non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// is dlist after finishing the Scan function
	gptr<gptr<T>> 		hp_temp;	// Temporary variable
	gptr<T>			addr;		// Temporary variable

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
        while (!list_delet.empty())
	{
		addr = list_delet.back();
		list_delet.pop_back();
		if (std::binary_search(plist.begin(), plist.end(), addr))
			new_dlist.push_back(addr);
		else
			list_recla.push_back(addr);
	}

	// Stage 4
	list_delet = std::move(new_dlist);
}

#endif /* MEMORY_HP_H */
