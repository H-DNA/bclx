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
		memory();
		~memory();
                gptr<T> malloc();		//allocates global memory
                void free(const gptr<T> &);	//deallocates global memory

	private:
		#define			THRESHOLD	BCL::nprocs()

                gptr<elem_dang<T>>	pool;           //allocates global memory
                gptr<elem_dang<T>>	poolRep;    	//deallocates global memory
                uint64_t		capacity;       //contains global memory capacity (bytes)
		sds::list<gptr<T>>	listAlloc;	//contains allocated elems
		sds::list<gptr<T>>	listRecla;	//contains reclaimed elems

		void scan();
	};

} /* namespace dang */

} /* namespace dds */

template <typename T>
dds::dang::memory<T>::memory()
{
        pool = poolRep = BCL::alloc<elem_dang<T>>(ELEM_PER_UNIT);
        capacity = pool.ptr + ELEM_PER_UNIT * sizeof(elem_dang<T>);
}

template <typename T>
dds::dang::memory<T>::~memory()
{
        BCL::dealloc<elem_dang<T>>(poolRep);
}

template <typename T>
dds::gptr<T> dds::dang::memory<T>::malloc()
{
	sds::elem<gptr<T>>	*addr;
	gptr<T>			res;
	gptr<bool>		temp;

	if (listAlloc.size() >= THRESHOLD)
		scan();

        //determine the global address of the new element
	if (listRecla.remove(addr) != EMPTY)
	{
		temp = {addr->value.rank, addr->value.ptr - sizeof(temp.rank)};
		BCL::store(false, temp);

		res = addr->value;
		listAlloc.insert(addr);
	}
	else //the list of reclaimed global memory is empty
	{
                if (pool.ptr < capacity)
		{
			temp = {pool.rank, pool.ptr};
			BCL::store(false, temp);

			res = {pool.rank, pool.ptr + sizeof(res.rank)};
			pool++;
			listAlloc.insert(res);
		}
                else //if (pool.ptr == capacity)
                {
                        //try one more to reclaim global memory
                        scan();
                        if (listRecla.remove(addr) != EMPTY)
                        {
				temp = {addr->value.rank, addr->value.ptr - sizeof(temp.rank)};
                        	BCL::store(false, temp);

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
	gptr<bool>	temp;

	temp.rank = addr.rank;
	temp.ptr = addr.ptr - sizeof(addr.rank);
	BCL::aput_sync(true, temp);
}

template <typename T>
void dds::dang::memory<T>::scan()
{
	gptr<elem_dang<T>>	temp;
        sds::elem<gptr<T>> 	*prev = nullptr,
				*curr = listAlloc.front(),
				*newCurr;

	temp.rank = BCL::rank();
        while (curr != nullptr)
        {
		temp.ptr = curr->value.ptr - sizeof(curr->value.rank);
                if (BCL::aget_sync(temp).taken)
		{
			newCurr = curr->next;
			listAlloc.remove(prev, curr);
			listRecla.insert(curr);
			curr = newCurr;
		}
		else
		{
			prev = curr;
                	curr = curr->next;
		}
        }
}

#endif /* MEMORY_DANG_H */
