#ifndef POOL_UBD_SPSC_H
#define POOL_UBD_SPSC_H

#include <vector>		// std::vector...
#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...

namespace dds
{

using namespace bclx;

/* Interface */

class pool_ubd_spsc
{
public:
	pool_ubd_spsc(const uint64_t& 			host,
			const uint64_t& 		obj_size,
			const bool&			is_small,
			const bool&			is_sync,
			const gptr<gptr<block>>&	hp);
	~pool_ubd_spsc();
	gptr<gptr<block>> get_head_ptr() const;
	void clear();
	void put(const gptr<void>& ptr);
	void put(const std::vector<gptr<void>>& ptrs);
	bool get(list_seq2& list);

private:
	gptr<gptr<block>>	head_ptr;
	gptr<block>		head_addr;
}; /* namespace pool_ubd_spsc */

} /* namespace dds */

/* Implementation */

dds::pool_ubd_spsc::pool_ubd_spsc(const uint64_t&		host,
				const uint64_t&			obj_size,
				const bool&			is_small,
				const bool&			is_sync,
				const gptr<gptr<block>>&	hp)
{
	if (BCL::rank() == host)
	{
		head_ptr = BCL::alloc<gptr<block>>(1);
		if (head_ptr == nullptr)
		{
			printf("[%lu]pool_ubd_spsc::pool_ubd_spsc\n", BCL::rank());
			return;
		}

		gptr<char> dummy = BCL::alloc<char>(obj_size);
		if (dummy == nullptr)
		{
			printf("[%lu]pool_ubd_spsc::pool_ubd_spsc\n", BCL::rank());
				return;
		}

		if (is_small)	// the block is SMALL
			head_addr = {dummy.rank, dummy.ptr + 8};
		else	// the block is LARGE
			head_addr = {dummy.rank, dummy.ptr + 16};
		bclx::store(head_addr, head_ptr);	// local access

		if (is_sync)
		{
			// broadcast
			head_ptr = BCL::broadcast(head_ptr, host);
			head_addr = BCL::broadcast(head_addr, host);
		}
		else { /* do nothing */ }
	}
	else // if (BCL::rank() != host)
	{
	
		if (is_sync)
		{	
			// broadcast
			head_ptr = BCL::broadcast(head_ptr, host);
			head_addr = BCL::broadcast(head_addr, host);
		}
		else // if (!is_sync)
		{
			head_ptr = hp;
			head_addr = bclx::rget_sync(head_ptr);	// remote access
		}
	}
}		

dds::pool_ubd_spsc::~pool_ubd_spsc() { /* TODO */ }

bclx::gptr<bclx::gptr<bclx::block>> dds::pool_ubd_spsc::get_head_ptr() const
{
	return head_ptr;
}

void dds::pool_ubd_spsc::clear() { /* TODO */ }

void dds::pool_ubd_spsc::put(const gptr<void>& ptr)
{
	// Link the block to the remote list
	block tmp;
	tmp.next = {head_addr.rank, head_addr.ptr};
	bclx::rput_sync(tmp, {ptr.rank, ptr.ptr});	// remote access
	
	// Update the head pointer
	bclx::aput_sync({ptr.rank, ptr.ptr}, head_ptr);	// remote access
}

void dds::pool_ubd_spsc::put(const std::vector<gptr<void>>& ptrs)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<block>	buffer;
	block			tmp;
	for (uint64_t i = 0; i < ptrs.size(); ++i)
	{
		disp.push_back(int64_t(ptrs[i].ptr) - int64_t(ptrs[0].ptr));
		if (i != ptrs.size() - 1)
			tmp.next = {ptrs[i + 1].rank, ptrs[i + 1].ptr};
		else // if (i == ptrs.size() - 1)
			tmp.next = head_addr;
		buffer.push_back(tmp);
	}

	// Link the remote global pointers and the head pointer of the pool together
	gptr<rll_t>	base = {ptrs[0].rank, ptrs[0].ptr};
	rll_t		rll(disp);
	bclx::rput_sync(buffer, base, rll);	// remote

	// Cache the head address of the pool
	head_addr = {base.rank, base.ptr};

	// Update the head pointer of the pool
	bclx::aput_sync(head_addr, head_ptr);	// remote
}

bool dds::pool_ubd_spsc::get(list_seq2& list)
{
	// Get the current head address of the pool
	gptr<block> curr = bclx::aget_sync(head_ptr);	// local

	// Check if the head has been changed by the producer
	if (curr == head_addr)
		return false;	// the queue is empty now

	// Update the list
	list.set(bclx::load(curr).next, head_addr);	// local

	// Cache the head of the pool
	head_addr = curr;

	return true;
}

/**/

#endif /* POOL_UBD_SPSC_H */
