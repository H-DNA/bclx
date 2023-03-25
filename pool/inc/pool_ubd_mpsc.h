#ifndef POOL_UBD_MPSC_H
#define POOL_UBD_MPSC_H

#include <vector>		// std::vector...
#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...

namespace dds
{

using namespace bclx;

class pool_ubd_mpsc
{
public:
	pool_ubd_mpsc(const gptr<gptr<block>>& hp);
	pool_ubd_mpsc(const uint64_t&	host,
			const uint64_t&	obj_size,
			const bool&	is_small,
			const bool&	is_sync);
	~pool_ubd_mpsc();
	void clear();
	void put(const std::vector<gptr<void>>& vals);
	bool get(list_seq2& slist);

	gptr<gptr<block>>	head_ptr;

private:
	gptr<block>		head_addr;
}; /* pool_ubd_mpsc */

} /* namespace dds */

/* Implementation of pool_ubd_mpsc */

dds::pool_ubd_mpsc::pool_ubd_mpsc(const gptr<gptr<block>>& hp)
		: head_ptr{hp} {}

dds::pool_ubd_mpsc::pool_ubd_mpsc(const uint64_t&	host,
				const uint64_t&		obj_size,
				const bool&		is_small,
				const bool&		is_sync)
{
	// initialize the pool locally
	if (BCL::rank() == host)
	{
		head_ptr = BCL::alloc<gptr<block>>(1);
		if (head_ptr == nullptr)
		{
			printf("[%lu]ERROR: pool_ubd_mpsc::pool_ubd_mpsc\n", BCL::rank());
			return;
		}

		gptr<char> dummy = BCL::alloc<char>(obj_size);
		if (dummy == nullptr)
		{
			printf("[%lu]ERROR: pool_ubd_mpsc::pool_ubd_mpsc\n", BCL::rank());
			return;
		}

		if (is_small)	// the block is SMALL
			head_addr = {dummy.rank, dummy.ptr + 8};
		else	// the block is LARGE
			head_addr = {dummy.rank, dummy.ptr + 16};
		bclx::store(head_addr, head_ptr);
	}

	// broadcast
	if (is_sync)	
		head_ptr = BCL::broadcast(head_ptr, host);
}

dds::pool_ubd_mpsc::~pool_ubd_mpsc() {}

void dds::pool_ubd_mpsc::clear()
{
	/*if (BCL::rank() == HOST)
	{
		BCL::dealloc<block<T>>(dummy);
		BCL::dealloc<gptr<block>>(head_ptr);
	}*/
}

void dds::pool_ubd_mpsc::put(const std::vector<gptr<void>>& vals)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<block>	buffer;
	block			tmp;
	for (uint64_t i = 0; i < vals.size() - 1; ++i)
	{
		disp.push_back(int64_t(vals[i].ptr) - int64_t(vals[0].ptr));
		tmp.next = {vals[i + 1].rank, vals[i + 1].ptr};
		buffer.push_back(tmp);
	}

	// Link the remote global pointers
	gptr<rll_t>	base = {vals[0].rank, vals[0].ptr};
	rll_t		rll(disp);
	bclx::rput_sync(buffer, base, rll);	// remote
	
	// Get the current head address
	gptr<block> curr = bclx::aget_sync(head_ptr);	// remote

	// Update the last remote global pointer and the head pointer of the pool
	gptr<gptr<block>> last = {vals[vals.size() - 1].rank, vals[vals.size() - 1].ptr};
	gptr<block> result;
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

bool dds::pool_ubd_mpsc::get(list_seq2& slist)
{
	// Get the current head address of the pool
	gptr<block> curr = bclx::aget_sync(head_ptr);	// local

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
