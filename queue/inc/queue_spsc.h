#ifndef QUEUE_SPSC_H
#define QUEUE_SPSC_H

#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...
#include <utility>	// std::move...

namespace dds
{

template<typename T>
class queue_spsc
{
public:
	queue_spsc(const uint64_t&	host,
		const uint64_t&		cap);
	~queue_spsc();
	void clear();
	void enqueue(const std::vector<T>& vals);
	bool dequeue(std::vector<T>& vals);

private:
	const uint64_t		HOST;
	const uint64_t		CAPACITY;
	uint64_t		head;
	uint64_t		tail_local;
	bclx::gptr<uint64_t>	tail;
	bclx::gptr<T>		items;
};

} /* namespace dds */

template<typename T>
dds::queue_spsc<T>::queue_spsc(const uint64_t&	host,
				const uint64_t& cap)
	: HOST{host}, CAPACITY{cap}, head{0}, tail_local{0}
{
	if (BCL::rank() == HOST)
	{
		tail = BCL::alloc<uint64_t>(1);
		bclx::store(uint64_t(0), tail);

		items = BCL::alloc<T>(CAPACITY);
	}

	// synchronize	
	tail = BCL::broadcast(tail, HOST);
	items = BCL::broadcast(items, HOST);
}

template<typename T>
dds::queue_spsc<T>::~queue_spsc() {}

template<typename T>
void dds::queue_spsc<T>::clear()
{
	if (BCL::rank() == HOST)
	{
		BCL::dealloc<uint64_t>(tail);
		BCL::dealloc<T>(items);
	}
}

template<typename T>
void dds::queue_spsc<T>::enqueue(const std::vector<T>& vals)
{
	uint64_t offset = tail_local % CAPACITY;
	bclx::gptr<T> location = items + offset;
	const T* array = &vals[0];
	if (offset + vals.size() <= CAPACITY)
		bclx::rput_sync(array, location, vals.size());	// remote
	else // if (offset + vals.size() > CAPACITY)
	{
		uint64_t size = CAPACITY - offset;
		bclx::rput_sync(array, location, size);	// remote;
		uint64_t size2 = vals.size() - size;
		bclx::rput_sync(array + size, items, size2);	// remote
	}
	tail_local += vals.size();
	bclx::aput_sync(tail_local, tail);	// remote
}

template<typename T>
bool dds::queue_spsc<T>::dequeue(std::vector<T>& vals)
{
	tail_local = bclx::aget_sync(tail);	// local
	uint64_t length = tail_local - head;
	if (length == 0)
		return false;	// the queue is empty now
	uint64_t offset = head % CAPACITY;
	bclx::gptr<T> location = items + offset;
	T* array = new T[length];
	if (offset + length <= CAPACITY)
		bclx::load(location, array, length);	// local
	else // if (offset + length > CAPACITY)
	{
		uint64_t size = CAPACITY - offset;	// local
		bclx::load(location, array, size);
		uint64_t size2 = length - size;
		bclx::load(location, array + size, size2);	// local
	}
	for (uint64_t i = 0; i < length; ++i)
		vals.push_back(array[i]);
	delete[] array;
	head += vals.size();
	return true;
}

#endif /* QUEUE_SPSC_H */
