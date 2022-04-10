#ifndef MEMORY_DANG6_H
#define MEMORY_DANG6_H

#include <cstdint>	// uint64_t...
#include <vector>	// std::vector...
#include <algorithm>	// std::sort...
#include <utility>	// std::move...

namespace dds
{

namespace dang6
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
	gptr<T> reserve(const gptr<gptr<T>>&);	// try to protect a global pointer from reclamation
	void unreserve(const gptr<T>&);		// stop protecting a global pointer

private:
	const gptr<T>		NULL_PTR 	= nullptr;
        const uint32_t		HPS_PER_UNIT 	= 1;
        const uint64_t		HP_TOTAL	= BCL::nprocs() * HPS_PER_UNIT;
        const uint64_t		HP_WINDOW	= HP_TOTAL * 2;

	gptr<T>         	pool;		// allocate global memory
	gptr<T>         	pool_rep;	// deallocate global memory
	uint64_t		capacity;	// contain global memory capacity (bytes)
	gptr<gptr<T>>		reservation;	// be an array of hazard pointers of the calling unit
	std::vector<gptr<T>>	list_ret;	// contain retired elems
	std::vector<gptr<T>>	list_rec;	// contain reclaimed elems
};

} /* namespace dang6 */

} /* namespace dds */

template<typename T>
dds::dang6::memory<T>::memory()
{
	if (BCL::rank() == MASTER_UNIT)
		mem_manager = "DANG6";

        gptr<gptr<T>> temp = reservation = BCL::alloc<gptr<T>>(HPS_PER_UNIT);
	for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
	{
		BCL::store(NULL_PTR, temp);
		++temp;
	}

	pool = pool_rep = BCL::alloc<T>(TOTAL_OPS);
        capacity = pool.ptr + TOTAL_OPS * sizeof(T);
}

template<typename T>
dds::dang6::memory<T>::~memory()
{
        BCL::dealloc<T>(pool_rep);
	BCL::dealloc<gptr<T>>(reservation);
}

template<typename T>
dds::gptr<T> dds::dang6::memory<T>::malloc()
{
        // determine the global address of the new element
        if (!list_rec.empty())
	{
		// tracing
		#ifdef	TRACING
			++elem_ru;
		#endif

		gptr<T> addr = list_rec.back();
		list_rec.pop_back();
                return addr;
	}
        else if (pool.ptr < capacity)	// the list of reclaimed global memory is empty
		return pool++;
	else
		return nullptr;
}

template<typename T>
void dds::dang6::memory<T>::free(const gptr<T>& addr)
{
	list_ret.push_back(addr);
}

template<typename T>
void dds::dang6::memory<T>::op_begin()
{
	/* No-op */
}

template<typename T>
void dds::dang6::memory<T>::op_end()
{
	/* No-op */
}

template<typename T>
dds::gptr<T> dds::dang6::memory<T>::reserve(const gptr<gptr<T>>& ptr)
{
	gptr<T> ptr_old = aget_sync(ptr);	// one RMA
	if (ptr_old == nullptr)
		return nullptr;
	else // if (ptr_old != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (BCL::aget_sync(temp) == NULL_PTR)
			{
				gptr<T> ptr_new;
				while (true)
				{
					BCL::aput_sync(ptr_old, temp);
					ptr_new = BCL::aget_sync(ptr);	// one RMA
					if (ptr_new == nullptr)
					{
						BCL::aput_sync(NULL_PTR, temp);
						return nullptr;
					}
					else if (ptr_new == ptr_old)
						return ptr_old;
					else // if(ptr_new != ptr_old)
						ptr_old = ptr_new;
				}
			}
			else // if (BCL::aget_sync(temp) != NULL_PTR)
				++temp;
		printf("HP:Error\n");
		return nullptr;
	}
}

template<typename T>
void dds::dang6::memory<T>::unreserve(const gptr<T>& ptr)
{
	if (ptr == nullptr)
		return;
	else // if (ptr != nullptr)
	{
		gptr<gptr<T>> temp = reservation;
		for (uint32_t i = 0; i < HPS_PER_UNIT; ++i)
			if (BCL::aget_sync(temp) == ptr)
			{
				BCL::aput_sync(NULL_PTR, temp);
				return;
			}
			else // if (BCL::aget_sync(temp) != ptr)
				++temp;
		printf("HP:Error\n");
		return;
	}
}

#endif /* MEMORY_DANG6_H */
