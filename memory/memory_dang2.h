#ifndef MEMORY_DANG2_H
#define MEMORY_DANG2_H

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...

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
		gptr<T> malloc();		// allocates global memory
                void free(const gptr<T>&);	// deallocates global memory

	private:
                gptr<T>         	pool;		// allocates global memory
                gptr<T>         	pool_rep;	// deallocates global memory
                uint64_t		capacity;	// contains global memory capacity (bytes)
                std::vector<gptr<T>>	list_rec;	// contains reclaimed elems
        };

} /* namespace dang2 */

} /* namespace dds */

template <typename T>
dds::dang2::memory<T>::memory()
{
	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(T);
}

template <typename T>
dds::dang2::memory<T>::~memory()
{
        BCL::dealloc<T>(pool_rep);
}

template <typename T>
dds::gptr<T> dds::dang2::memory<T>::malloc()
{
        // determine the global address of the new element
        if (!list_rec.empty())
	{
		gptr<T> addr = list_rec.back();
		list_rec.pop_back();
                return addr;
	}
        else if (pool.ptr < capacity)	// the list of reclaimed global memory is empty
		return pool++;
	else // if (pool.ptr == capacity)
		return nullptr;
}

template <typename T>
void dds::dang2::memory<T>::free(const gptr<T>& addr)
{
	list_rec.push_back(addr);
}

#endif /* MEMORY_DANG2_H */
