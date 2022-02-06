#ifndef MEMORY_NMR_H
#define MEMORY_NMR_H

#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...

namespace dds
{

namespace nmr
{

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
	gptr<T>		pool;			// allocate PGAS
	gptr<T>		pool_rep;		// deallocate PGAS
	uint64_t	capacity;		// contain global memory capacity (bytes)
};

} /* namespace dang3 */

} /* namespace dds */

template<typename T>
dds::nmr::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "NMR";

	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(T);
}

template<typename T>
dds::nmr::memory<T>::~memory()
{
        BCL::dealloc<T>(pool_rep);
}

template<typename T>
dds::gptr<T> dds::nmr::memory<T>::malloc()
{
        // determine the global address of the new element
        if (pool.ptr < capacity)
		return pool++;
	return nullptr;
}

template<typename T>
void dds::nmr::memory<T>::free(const gptr<T>& ptr)
{
	/* No-op */
}

template<typename T>
void dds::nmr::memory<T>::op_begin()
{
	/* No-op */
}

template<typename T>
void dds::nmr::memory<T>::op_end()
{
	/* No-op */
}

template<typename T>
bool dds::nmr::memory<T>::try_reserve(gptr<T>& ptr, const gptr<gptr<T>>& atom)
{
	return true;
}

template<typename T>
void dds::nmr::memory<T>::unreserve(const gptr<T>& ptr)
{
	/* No-op */
}

#endif /* MEMORY_NMR_H */
