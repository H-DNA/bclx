#ifndef MEMORY_NMR_H
#define MEMORY_NMR_H

#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...

namespace dds
{

namespace nmr
{

/* Macros */
using namespace bclx;

/* Datatypes */
template<typename T>
class memory
{
public:
	memory();
	~memory();
	gptr<T> malloc();			// allocate global memory
	void free(const gptr<T>&);		// deallocate global memory
	void retire(const gptr<T>&);		// retire a global pointer
	void op_begin();			// indicate the beginning of a concurrent operation
	void op_end();				// indicate the end of a concurrent operation
	bool try_reserve(const gptr<gptr<T>>&,	// try to to protect a global pointer from reclamation
			gptr<T>&);
	gptr<T> reserve(const gptr<gptr<T>>&);	// try to protect a global pointer from reclamation
	void unreserve(const gptr<T>&);		// stop protecting a global pointer

private:
	const uint64_t	WINDOW = 2 * BCL::nprocs();

	list_seq<T>	pool_mem;		// allocate PGAS
	gptr<T>		pool_rep;		// deallocate PGAS
	list_seq<T>	lheap;
};

} /* namespace nmr */

} /* namespace dds */

template<typename T>
dds::nmr::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "NMR";

	pool_rep = BCL::alloc<T>(TOTAL_OPS);
	if (pool_rep == nullptr)
	{
		printf("[%lu]ERROR: memory.memory\n", BCL::rank());
		return;
	}
	pool_mem.set(pool_rep, TOTAL_OPS);
}

template<typename T>
dds::nmr::memory<T>::~memory()
{
        BCL::dealloc<T>(pool_rep);
}

template<typename T>
bclx::gptr<T> dds::nmr::memory<T>::malloc()
{
	// if lheap.contig is not empty, return a gptr<T> from it
	if (!lheap.empty())
		return lheap.pop();

	// otherwise, get elems from the memory pool
	if (!pool_mem.empty())
	{
		gptr<T> ptr = pool_mem.pop(WINDOW);
		lheap.set(ptr, WINDOW);
		
		return lheap.pop();
	}

	// otherwise, return nullptr
	printf("[%lu]ERROR: memory.malloc\n", BCL::rank());
	return nullptr;
}

template<typename T>
void dds::nmr::memory<T>::free(const gptr<T>& ptr) {}

template<typename T>
void dds::nmr::memory<T>::retire(const gptr<T>& ptr) {}

template<typename T>
void dds::nmr::memory<T>::op_begin() {}

template<typename T>
void dds::nmr::memory<T>::op_end() {}

template<typename T>
bool dds::nmr::memory<T>::try_reserve(const gptr<gptr<T>>& ptr, gptr<T>& val_old)
{
	return true;
}

template<typename T>
bclx::gptr<T> dds::nmr::memory<T>::reserve(const gptr<gptr<T>>& ptr)
{
	return aget_sync(ptr);	// one RMA
}

template<typename T>
void dds::nmr::memory<T>::unreserve(const gptr<T>& ptr) {}

#endif /* MEMORY_NMR_H */
