#ifndef QUEUE_SPSC_H
#define QUEUE_SPSC_H

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
	uint64_t	host;
	uint64_t	capacity;
	uint64_t	head;
	uint64_t	tail_local;
	gptr<uint64_t>	tail;
	gptr<T>		items;
};

} /* namespace dds */

template<typename T>
dds::queue_spsc<T>::queue_spsc(const uint64_t&	host,
				const uint64_t& cap)
	: host{host}, capacity{cap}, head{0}, tail_local{0}
{
	if (BCL::rank() == host)
	{
		tail = BCL::alloc<uint64_t>(1);
		BCL::store(uint64_t(0), tail);

		items = BCL::alloc<T>(cap);
	}

	// synchronize	
	tail = BCL::broadcast(tail, host);
	items = BCL::broadcast(items, host);
}

template<typename T>
dds::queue_spsc<T>::~queue_spsc()
{
	/* No-op */
}

template<typename T>
void dds::queue_spsc<T>::clear()
{
	if (BCL::rank() == host)
	{
		BCL::dealloc<uint64_t>(tail);
		BCL::dealloc<T>(items);
	}
}

template<typename T>
void dds::queue_spsc<T>::enqueue(const std::vector<T>& vals)
{
	gptr<T> temp = items + tail_local % capacity;
	const T* array = &vals[0];
	BCL::rput_sync(array, temp, vals.size());	// remote
	tail_local += vals.size();
	BCL::aput_sync(tail_local, tail);	// remote
}

template<typename T>
bool dds::queue_spsc<T>::dequeue(std::vector<T>& vals)
{
	tail_local = BCL::aget_sync(tail);	// local
	uint64_t size = tail_local - head;
	if (size == 0)
		return false;	// the queue is empty now
	gptr<T> temp = items + head % capacity;
	T* array = &vals[0];
	BCL::load(temp, array, size);	// local
	head += vals.size();
	return true;
}

#endif /* QUEUE_SPSC_H */
