#ifndef MEMORY_DANG_H
#define MEMORY_DANG_H

namespace dds
{

namespace dang
{

	template <typename T>
	struct elem_dang
	{
		bool 	taken;
		T	elemD;
	};

	template <typename T>
	class memory
	{
	public:
		gptr<gptr<T>>	hp;		//Hazard pointer

		memory();
		~memory();
                gptr<T> malloc();		//allocates global memory
                void free(const gptr<T> &);	//deallocates global memory

	private:
		const gptr<T>   NULL_PTR        = nullptr;
		const uint64_t  HPS_PER_UNIT    = 1;
		#define         HP_TOTAL        BCL::nprocs() * HPS_PER_UNIT
		#define         HP_WINDOW       HP_TOTAL * 2

                gptr<elem_dang<T>>		pool;           //allocates global memory
                gptr<elem_dang<T>>		poolRep;    	//deallocates global memory
                uint64_t			capacity;       //contains global memory capacity (bytes)
		sds::list<gptr<T>>		listAlloc;	//contains allocated elems
		sds::list<gptr<T>>		listRecla;	//contains reclaimed elems

		void scan();
	};

} /* namespace dang */

} /* namespace dds */

template <typename T>
dds::dang::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG";

        hp = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
        BCL::store(NULL_PTR, hp);

        pool = poolRep = BCL::alloc<elem_dang<T>>(ELEMS_PER_UNIT);
        capacity = pool.ptr + ELEMS_PER_UNIT * sizeof(elem_dang<T>);
}

template <typename T>
dds::dang::memory<T>::~memory()
{
        BCL::dealloc<elem_dang<T>>(poolRep);
	BCL::dealloc<gptr<T>>(hp);
}

template <typename T>
dds::gptr<T> dds::dang::memory<T>::malloc()
{
	sds::elem<gptr<T>>	*addr;
	gptr<T>			res;
	gptr<bool>		temp;

        //determine the global address of the new element
	if (listRecla.remove(addr) != EMPTY)
	{
		//tracing
		#ifdef  TRACING
			++elem_re;
		#endif

		temp = {addr->value.rank, addr->value.ptr - sizeof(temp.rank)};
		BCL::store(true, temp);

		res = addr->value;
		listAlloc.insert(addr);
	}
	else //the list of reclaimed global memory is empty
	{
                if (pool.ptr < capacity)
		{
			temp = {pool.rank, pool.ptr};
			BCL::store(true, temp);

			res = {pool.rank, pool.ptr + sizeof(res.rank)};
			listAlloc.insert(res);

			pool++;
		}
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

				temp = {addr->value.rank, addr->value.ptr - sizeof(temp.rank)};
                        	BCL::store(true, temp);

				res = addr->value;
                        	listAlloc.insert(addr);
			}
                        else //the list of reclaimed global memory is empty
                                res = nullptr;
                }
	}

	return res;
}

template <typename T>
void dds::dang::memory<T>::free(const gptr<T> &addr)
{
	gptr<bool> temp = {addr.rank, addr.ptr - sizeof(addr.rank)};
	BCL::aput_sync(false, temp);

        if (listAlloc.size() >= HP_WINDOW)
                scan();
}

template <typename T>
void dds::dang::memory<T>::scan()
{
	uint64_t 		p = 0;			//contains the number of elems in @plist
	gptr<T> 		hptr,			//Temporary variable
				plist[HP_TOTAL];	//contains non-null hazard pointers
	sds::list<gptr<T>>	new_dlist;		//is dlist after finishing the Scan function
	gptr<gptr<T>> 		hpTemp;			//Temporary variable
	sds::elem<gptr<T>>      *addr;			//Temporary variable
	gptr<elem_dang<T>>	temp;

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
	temp.rank = BCL::rank();
        while (listAlloc.remove(addr) != EMPTY)
	{
		temp.ptr = addr->value.ptr - sizeof(addr->value.rank);
		if (BCL::aget_sync(temp).taken || sds::binary_search(addr->value, plist, p))
			new_dlist.insert(addr);
		else
			listRecla.insert(addr);
	}

	//Stage 4
	listAlloc.assign(new_dlist);
}

#endif /* MEMORY_DANG_H */
