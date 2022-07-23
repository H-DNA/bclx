#ifndef POOL_UBD_SPSC_H
#define POOL_UBD_SPSC_H

#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...
#include <utility>	// std::move...

namespace dds
{

template<typename T>
class pool_ubd_spsc
{
public:
	pool_ubd_spsc(const uint64_t& host);
	~pool_ubd_spsc();
	void clear();
	void put(const std::vector<T>& vals);
	bool get(std::vector<T>& vals);

private:
	const uint64_t	HOST;
	gptr<T>		head;
	gptr<T>		prev;

	//uint64_t	head;
	//uint64_t	tail_local;
	//gptr<uint64_t>	tail;
	//gptr<T>		items;
};

} /* namespace dds */

template<typename T>
dds::pool_ubd_spsc<T>::pool_ubd_spsc(const uint64_t& host)
	: HOST{host}
{
	head = BCL::alloc<T>(1);
	if (BCL::rank() == HOST)
		BCL::store(nullptr, head);

	// synchronize	
	head = BCL::broadcast(head, HOST);
}

template<typename T>
dds::pool_ubd_spsc<T>::~pool_bd_spsc()
{
	/* No-op */
}

template<typename T>
void dds::pool_ubd_spsc<T>::clear()
{
	BCL::dealloc<T>(head);
}

template<typename T>
void dds::pool_ubd_spsc<T>::put(const std::vector<T>& vals)
{
	uint64_t offset = tail_local % CAPACITY;
	gptr<T> location = items + offset;
	const T* array = &vals[0];
	if (offset + vals.size() <= CAPACITY)
		BCL::rput_sync(array, location, vals.size());	// remote
	else // if (offset + vals.size() > CAPACITY)
	{
		uint64_t size = CAPACITY - offset;
		BCL::rput_sync(array, location, size);	// remote;
		uint64_t size2 = vals.size() - size;
		BCL::rput_sync(array + size, items, size2);	// remote
	}
	tail_local += vals.size();
	BCL::aput_sync(tail_local, tail);	// remote
}

template<typename T>
bool dds::pool_ubd_spsc<T>::get(std::vector<T>& vals)
{
	tail_local = BCL::aget_sync(tail);	// local
	uint64_t length = tail_local - head;
	if (length == 0)
		return false;	// the queue is empty now
	uint64_t offset = head % CAPACITY;
	gptr<T> location = items + offset;
	T* array = new T[length];
	if (offset + length <= CAPACITY)
		BCL::load(location, array, length);	// local
	else // if (offset + length > CAPACITY)
	{
		uint64_t size = CAPACITY - offset;	// local
		BCL::load(location, array, size);
		uint64_t size2 = length - size;
		BCL::load(location, array + size, size2);	// local
	}
	for (uint64_t i = 0; i < length; ++i)
		vals.push_back(array[i]);
	delete[] array;
	head += vals.size();
	return true;
}

#endif /* POOL_UBD_SPSC_H */
