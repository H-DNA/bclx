#ifndef MEMORY_DANG3_H
#define MEMORY_DANG3_H

namespace dds
{

namespace dang3
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
                gptr<T>		pool;		//allocates global memory
                gptr<T>		poolRep;	//deallocates global memory
                uint64_t	capacity;	//contains global memory capacity (bytes)
        };

} /* namespace dang3 */

} /* namespace dds */

template <typename T>
dds::dang3::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG3";

	pool = poolRep = BCL::alloc<T>(ELEMS_PER_UNIT);
        capacity = pool.ptr + ELEMS_PER_UNIT * sizeof(T);
}

template <typename T>
dds::dang3::memory<T>::~memory()
{
        BCL::dealloc<T>(poolRep);
}

template <typename T>
dds::gptr<T> dds::dang3::memory<T>::malloc()
{
        //determine the global address of the new element
        if (pool.ptr < capacity)
		return pool++;
	else //if (pool.ptr == capacity)
		return nullptr;
}

template <typename T>
void dds::dang3::memory<T>::free(const gptr<T> &addr)
{
	//do nothing
}

#endif /* MEMORY_DANG3_H */
