#ifndef POOL_UBD_SPSC_H
#define POOL_UBD_SPSC_H

#include <vector>		// std::vector...
#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...

namespace dds
{

using namespace bclx;

struct header
{
	gptr<header>	next;
};

template<typename T>
struct block
{
	header	head;
	T	data;
};

template<typename T>
class pool_ubd_spsc
{
public:
	pool_ubd_spsc(const uint64_t& host);
	~pool_ubd_spsc();
	void clear();
	void put(const std::vector<gptr<T>>& vals);
	bool get(gptr<header>& vals_head, gptr<header>& vals_tail);

private:
	const header		NULL_PTR;
	const uint64_t		HOST;
	gptr<gptr<header>>	head_ptr;
	gptr<header>		head_addr;
	gptr<block<T>>		dummy;
};

template<typename T>
pool_ubd_spsc<T>::pool_ubd_spsc(const uint64_t& host)
				: HOST{host}
{
	// initialize the pool locally
	if (BCL::rank() == HOST)
	{
		head_ptr = BCL::alloc<gptr<header>>(1);
		dummy = BCL::alloc<block<T>>(1);
		head_addr = {dummy.rank, dummy.ptr};
		store(head_addr, head_ptr);
	}

	// broadcast
	head_addr = BCL::broadcast(head_addr, HOST);	
	head_ptr = BCL::broadcast(head_ptr, HOST);
}

template<typename T>
pool_ubd_spsc<T>::~pool_ubd_spsc() {}

template<typename T>
void pool_ubd_spsc<T>::clear()
{
	if (BCL::rank() == HOST)
	{
		BCL::dealloc<block<T>>(dummy);
		BCL::dealloc<gptr<header>>(head_ptr);
	}
}

template<typename T>
void pool_ubd_spsc<T>::put(const std::vector<gptr<T>>& vals)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<header>	buffer;
	header			tmp;
	for (uint64_t i = 0; i < vals.size(); ++i)
	{
		disp.push_back(int64_t(vals[i].ptr) - int64_t(vals[0].ptr));
		if (i != vals.size() - 1)
			tmp.next = {vals[i + 1].rank, vals[i + 1].ptr - sizeof(header)};
		else // if (i == vals.size() - 1)
			tmp.next = head_addr;
		buffer.push_back(tmp);
	}

	// Link the remote global pointers and the head pointer of the pool together
	gptr<rll_t>	base = {vals[0].rank, vals[0].ptr - sizeof(header)};
	rll_t		rll(disp);
	rput_sync(buffer, base, rll);	// remote

	// Cache the head address of the pool
	head_addr = {base.rank, base.ptr};

	// Update the head pointer of the pool
	aput_sync(head_addr, head_ptr);	// remote
}

template<typename T>
bool pool_ubd_spsc<T>::get(gptr<header>&	vals_head,
				gptr<header>&	vals_tail)
{
	// Get the current head address of the pool
	gptr<header> curr = aget_sync(head_ptr);	// local

	// Check if the head has been changed by the producer
	if (curr == head_addr)
		return false;	// the queue is empty now

	// Get the head of the list returned
	vals_head = load(curr).next;	// local

	// Get the tail of the list returned
	vals_tail = head_addr;

	// Cache the head of the pool
	head_addr = curr;

	return true;
}

} /* namespace dds */

#endif /* POOL_UBD_SPSC_H */
