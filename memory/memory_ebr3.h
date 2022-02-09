#ifndef MEMORY_EBR3_H
#define MEMORY_EBR3_h

#include <cstdint>	// uint64_t...
#include <limits>	// std::numeric_limits...
#include <vector>	// std::vector...

namespace dds
{

namespace ebr3
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
	const uint64_t		MIN		= std::numeric_limits<uint64_t>::min();
	const uint64_t		EPOCH_FREQ	= BCL::nprocs();	// freq. of increasing epoch
	const uint64_t		EMPTY_FREQ	= BCL::nprocs() * 2;	// freq. of reclaiming retired

	gptr<T>			pool;		// allocate global memory
	gptr<T>			pool_rep;	// deallocate global memory
	uint64_t		capacity;	// contain global memory capacity (bytes)
	uint64_t		epoch;		// a local epoch counter
	gptr<uint64_t>		reservation;	// a SWMR variable
	uint64_t		counter;	// a local counter
	std::vector<gptr<T>>	list_ret;	// contain retired elems
	std::vector<gptr<T>>	list_rec;	// contain reclaimed elems

	void empty();
}

} /* namespace ebr3 */

} /* namespace dds */

template<typename T>
dds::ebr3::memory<T>::memory()
{
	// TODO
}

template<typename T>
dds::ebr3::memory<T>::~memory()
{
	// TODO
}

template<typename T>
dds::gptr<T> dds::ebr3::memory<T>::malloc()
{
	// TODO
}

template<typename T>
void dds::ebr3::memory<T>::free(const gptr<T>& ptr)
{
	// TODO
}

template<typename T>
void dds::ebr3::memory<T>::op_begin()
{
	// TODO
}

template<typename T>
void dds::ebr3::memory<T>::op_end()
{
	// TODO
}

template<typename T>
bool dds::ebr3::memory<T>::try_reserve(gptr<T>&			ptr,
					const gptr<gptr<T>>& 	atom)
{
	// TODO
}

template<typename T>
void dds::ebr3::memory<T>::unreserve(const gptr<T>& ptr)
{
	// TODO
}

template<typename T>
void dds::ebr3::memory<T>::empty()
{
	//  TODO
}

#endif /* MEMORY_EBR3_H */
