#ifndef MEMORY_HP_H
#define MEMORY_HP_H

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
                const gptr<T>	NULL_PTR 	= nullptr;
        	const uint64_t	HPS_PER_UNIT 	= 1;
        	#define         HP_TOTAL	BCL::nprocs() * HPS_PER_UNIT
        	#define         HP_WINDOW	HP_TOTAL * 2

                gptr<T>         	pool;		//allocates global memory
                gptr<T>         	poolRep;	//deallocates global memory
                uint64_t		capacity;	//contains global memory capacity (bytes)
                sds::list<gptr<T>>      listDelet;      //contains deleted elems
                sds::list<gptr<T>>      listRecla;      //contains reclaimed elems

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
        gptr<T>		addr;

        //determine the global address of the new element
        if (listRecla.remove(addr) != EMPTY)
	{
		//tracing
		#ifdef	TRACING
			++elem_re;
		#endif

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
			if (listRecla.remove(addr) != EMPTY)
			{
				//tracing
				#ifdef  TRACING
					++elem_re;
				#endif

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
	listDelet.insert(addr);
	if (listDelet.size() >= HP_WINDOW)
		scan();
}

template <typename T>
void dds::hp::memory<T>::scan()
{	
	uint64_t 		p = 0;			//contains the number of elems in @plist
	gptr<T> 		hptr,			//Temporary variable
				plist[HP_TOTAL];	//contains non-null hazard pointers
	sds::list<gptr<T>>	new_dlist;		//is dlist after finishing the Scan function
	gptr<gptr<T>> 		hpTemp;			//Temporary variable
	sds::elem<gptr<T>>      *addr;			//Temporary variable

	//Stage 1
	hpTemp.ptr = hp.ptr;
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		hpTemp.rank = i;
		for (uint32_t j = 0; j < HPS_PER_UNIT; ++j)
		{
			hptr = BCL::aget_sync(hpTemp);
			if (hptr != nullptr)
				plist[p++] = hptr;
			++hpTemp;
		}
	}
	
	//Stage 2
	sds::heap_sort(plist, p);
	sds::remove_duplicates(plist, p);

	//Stage 3
        while (listDelet.remove(addr) != EMPTY)
	{
		if (sds::binary_search(addr->value, plist, p))
			new_dlist.insert(addr);
		else
			listRecla.insert(addr);
	}

	//Stage 4
	listDelet.assign(new_dlist);
}

#endif /* MEMORY_HP_H */
