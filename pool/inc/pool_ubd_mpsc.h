#ifndef POOL_UBD_MPSC_H
#define POOL_UBD_MPSC_H

#include <vector>		// std::vector...
#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...

namespace dds
{

using namespace bclx;

template<typename T>
class pool_ubd_mpsc
{
public:
	pool_ubd_mpsc(const uint64_t& host);
	~pool_ubd_mpsc();
	void clear();
	void put(const std::vector<gptr<T>>& vals);
	bool get(list_seq2<T>& slist);

private:
	const header		NULL_PTR;
	const uint64_t		HOST;
	gptr<gptr<header>>	head_ptr;
	gptr<header>		head_addr;
	gptr<block<T>>		dummy;
};

} /* namespace dds */

/* Implementation of pool_ubd_mpsc */

template<typename T>
dds::pool_ubd_mpsc<T>::pool_ubd_mpsc(const uint64_t& host)
				: HOST{host}
{
	// initialize the pool locally
	if (BCL::rank() == HOST)
	{
		head_ptr = BCL::alloc<gptr<header>>(1);
		dummy = BCL::alloc<block<T>>(1);
		head_addr = {dummy.rank, dummy.ptr};
		bclx::store(head_addr, head_ptr);
	}

	// broadcast	
	head_ptr = BCL::broadcast(head_ptr, HOST);
}

template<typename T>
dds::pool_ubd_mpsc<T>::~pool_ubd_mpsc() {}

template<typename T>
void dds::pool_ubd_mpsc<T>::clear()
{
	if (BCL::rank() == HOST)
	{
		BCL::dealloc<block<T>>(dummy);
		BCL::dealloc<gptr<header>>(head_ptr);
	}
}

template<typename T>
void dds::pool_ubd_mpsc<T>::put(const std::vector<gptr<T>>& vals)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<header>	buffer;
	header			tmp;
	for (uint64_t i = 0; i < vals.size() - 1; ++i)
	{
		disp.push_back(int64_t(vals[i].ptr) - int64_t(vals[0].ptr));
		tmp.next = {vals[i + 1].rank, vals[i + 1].ptr - sizeof(header)};
		buffer.push_back(tmp);
	}

	// Link the remote global pointers
	gptr<rll_t>	base = {vals[0].rank, vals[0].ptr - sizeof(header)};
	rll_t		rll(disp);
	bclx::rput_sync(buffer, base, rll);	// remote
	
	// Get the current head address
	gptr<header> curr = bclx::aget_sync(head_ptr);	// remote

	// Update the last remote global pointer and the head pointer of the pool
	gptr<gptr<header>> last = {vals[vals.size() - 1].rank, vals[vals.size() - 1].ptr - sizeof(header)};
	gptr<header> result;
	head_addr = {base.rank, base.ptr};
	while (true)
	{
		// Update the last remote global pointer
		bclx::rput_sync(curr, last);		// remote

		// Update the head pointer of the pool
		result = bclx::cas_sync(head_ptr, curr, head_addr);	// remote

		// Check if the cas has already succeeded
		if (result == curr)
			break;
		else // if (result != curr)
			curr = result;
	}
}

template<typename T>
bool dds::pool_ubd_mpsc<T>::get(list_seq2<T>& slist)
{
	// Get the current head address of the pool
	gptr<header> curr = bclx::aget_sync(head_ptr);	// local

	// Check if the head has been changed by the producer
	if (curr == head_addr)
		return false;	// the queue is empty now

	// Update the list
	slist.set(bclx::load(curr).next, head_addr);	// local

	// Cache the head of the pool
	head_addr = curr;

	return true;
}

/**/

#endif /* POOL_UBD_MPSC_H */
