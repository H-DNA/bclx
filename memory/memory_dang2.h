#ifndef MEMORY_DANG2_H
#define MEMORY_DANG2_H

namespace dds
{

namespace dang2
{

        template <typename T>
        class memory
        {
        public:
                memory();
                ~memory();
		gptr<T> malloc();		//allocates global memory
                void free(const gptr<T> &);	//deallocates global memory

	private:
                gptr<T>         	pool;		//allocates global memory
                gptr<T>         	poolRep;	//deallocates global memory
                uint64_t		capacity;	//contains global memory capacity (bytes)
                sds::list<gptr<T>>	listRec;	//contains reclaimed elems
        };

} /* namespace dang2 */

} /* namespace dds */

template <typename T>
dds::dang2::memory<T>::memory()
{
	pool = poolRep = BCL::alloc<T>(ELEMS_PER_UNIT);
        capacity = pool.ptr + ELEMS_PER_UNIT * sizeof(T);
}

template <typename T>
dds::dang2::memory<T>::~memory()
{
        BCL::dealloc<T>(poolRep);
}

template <typename T>
dds::gptr<T> dds::dang2::memory<T>::malloc()
{
	sds::elem<gptr<T>>	*addr;

        //determine the global address of the new element
        if (listRec.remove(addr) != EMPTY)
                return addr->value;
        else if (pool.ptr < capacity)	//the list of reclaimed global memory is empty
		return pool++;
	else //if (pool.ptr == capacity)
		return nullptr;
}

template <typename T>
void dds::dang2::memory<T>::free(const gptr<T> &addr)
{
	listRec.insert(addr);
}

#endif /* MEMORY_DANG2_H */
