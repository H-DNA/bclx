#ifndef QUEUE_SPMC_H
#define QUEUE_SPMC_H

#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...
#include <utility>	// std::move...
#include <algorith>	// std::min...

namespace dds
{

template<typename T>
class queue_spmc
{
public:
	queue_spmc(const uint64_t&	host,
		const uint64_t&		cap);
	~queue_spmc();
	void clear();
	void enqueue(const std::vector<T>& vals);
	bool dequeue(std::vector<T>& vals);

private:
	const uint64_t	BOUND_DEQUEUE	= 100;

	uint64_t	host;
	uint64_t	capacity;
	uint64_t	tail_local;
	gptr<uint64_t>	head;
	gptr<uint64_t>	tail;
	gptr<T>		items;
};

} /* namespace dds */

template<typename T>
dds::queue_spmc<T>::queue_spmc(const uint64_t&	host,
				const uint64_t& cap)
	: host{host}, capacity{cap}, tail_local{0}
{
	if (BCL::rank() == host)
	{
		head = BCL::alloc<uint64_t>(1);
		tail = BCL::alloc<uint64_t>(1);
		BCL::store(uint64_t(0), head);
		BCL::store(uint64_t(0), tail);

		items = BCL::alloc<T>(cap);
	}

	// synchronize	
	head = BCL::broadcast(head, host);
	tail = BCL::broadcast(tail, host);
	items = BCL::broadcast(items, host);
}

template<typename T>
dds::queue_spmc<T>::~queue_spmc()
{
	/* No-op */
}

template<typename T>
void dds::queue_spmc<T>::clear()
{
	if (BCL::rank() == host)
	{
		BCL::dealloc<uint64_t>(head);
		BCL::dealloc<uint64_t>(tail);
		BCL::dealloc<T>(items);
	}
}

template<typename T>
void dds::queue_spmc<T>::enqueue(const std::vector<T>& vals)
{
	gptr<T> temp = items + tail_local % capacity;
	const T* array = &vals[0];
	BCL::rput_sync(array, temp, vals.size());	// remote
	tail_local += vals.size();
	BCL::aput_sync(tail_local, tail);	// remote
}

template<typename T>
bool dds::queue_spmc<T>::dequeue(std::vector<T>& vals)
{
	uint64_t	size,
			head_old,
			head_new,
			result;

	tail_local = BCL::aget_sync(tail);	// local
	head_old = BCL::aget_sync(head);	// local
	while (true)	// lock-free loop
	{
		size = tail_local - head_old;
		if (size == 0)
			return false;	// the queue is empty now
		size_bound = std::min(size, BOUND_DEQUEUE);
		head_new += size_bound;
		result = BCL::cas_sync(head, head_old, head_new);	// local
		if (result == head_old)
			break;
		else // if (result != head_old)
			head_old = result;
	}
	gptr<T> temp = items + head_old % capacity;
	T* array = new T[size_bound];
	BCL::load(temp, array, size_bound);	// local
	for (uint64_t i = 0; i < size_bound; ++i)
		vals.push_back(array[i]);
	delete[] array;
	return true;
}

#endif /* QUEUE_SPMC_H */
