#ifndef MEMORY_HP_H
#define MEMORY_HP_H

#include <vector>
#include <algorithm>

namespace dds
{

namespace hp
{
        template <typename T>
        class memory
        {
        public:
                gptr<gptr<T>> 	hp;		//Hazard pointer

                memory();
                ~memory();
		gptr<T> malloc();		//allocates global memory
                void free(const gptr<T> &);	//deallocates global memory

	private:
                const gptr<T>		NULL_PTR 	= nullptr;
        	const uint8_t		HPS_PER_UNIT 	= 1;
        	const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
        	const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

                gptr<T>         	pool;		//allocates global memory
                gptr<T>         	poolRep;	//deallocates global memory
                uint64_t		capacity;	//contains global memory capacity (bytes)
                std::vector<gptr<T>>	listDelet;      //contains deleted elems
                std::vector<gptr<T>>	listRecla;      //contains reclaimed elems

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

	pool = poolRep = BCL::alloc<T>(ELEMS_PER_UNIT);
        capacity = pool.ptr + ELEMS_PER_UNIT * sizeof(T);

	listRecla.reserve(HP_WINDOW);
	listDelet.reserve(HP_WINDOW);
}

template <typename T>
dds::hp::memory<T>::~memory()
{
        BCL::dealloc<T>(poolRep);
	BCL::dealloc<gptr<T>>(hp);
}

template <typename T>
dds::gptr<T> dds::hp::memory<T>::malloc()
{
        //determine the global address of the new element
        if (!listRecla.empty())
	{
		//tracing
		#ifdef	TRACING
			++elem_re;
		#endif

		gptr<T> addr = listRecla.back();
		listRecla.pop_back();
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
			if (!listRecla.empty())
			{
				//tracing
				#ifdef  TRACING
					++elem_re;
				#endif

				gptr<T> addr = listRecla.back();
				listRecla.pop_back();
				return addr;
			}
			else //the list of reclaimed global memory is empty
				return nullptr;
		}
        }
}

template <typename T>
void dds::hp::memory<T>::free(const gptr<T> &addr)
{
	listDelet.push_back(addr);
	if (listDelet.size() >= HP_WINDOW)
		scan();
}

template <typename T>
void dds::hp::memory<T>::scan()
{	
	std::vector<gptr<T>>	plist;		// contains non-null hazard pointers
	std::vector<gptr<T>>	new_dlist;	// is dlist after finishing the Scan function
	gptr<gptr<T>> 		hpTemp;		// Temporary variable
	gptr<T>			addr;		// Temporary variable

	plist.reserve(HP_TOTAL);
	new_dlist.reserve(HP_TOTAL);

	// Stage 1
	hpTemp.ptr = hp.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		hpTemp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			addr = BCL::aget_sync(hpTemp);
			if (addr != nullptr)
				plist.push_back(addr);
			++hpTemp;
		}
	}
	
	// Stage 2
	std::sort(plist.begin(), plist.end());
	plist.resize(std::unique(plist.begin(), plist.end()) - plist.begin());

	// Stage 3
        while (!listDelet.empty())
	{
		addr = listDelet.back();
		listDelet.pop_back();
		if (std::binary_search(plist.begin(), plist.end(), addr))
			new_dlist.push_back(addr);
		else
			listRecla.push_back(addr);
	}

	// Stage 4
	listDelet.swap(new_dlist);
}

#endif /* MEMORY_HP_H */
