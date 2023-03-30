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
	pool_ubd_mpsc(const uint64_t&			host,
			const uint64_t&			size_class,
			const bool&			is_small,
			const bool&			is_sync,
			const gptr<gptr<block>>&	hp);
	~pool_ubd_mpsc();
	gptr<gptr<block>> get_head_ptr() const;
	void clear();
	void put(const gptr<void>& ptr);
	void put(const std::vector<gptr<void>>& ptrs);
	bool get(list_seq2& slist);

private:
	gptr<gptr<block>>	head_ptr;
	gptr<block>		head_addr;
}; /* pool_ubd_mpsc */

} /* namespace dds */

/* Implementation of pool_ubd_mpsc */

dds::pool_ubd_mpsc::pool_ubd_mpsc(const uint64_t&		host,
				const uint64_t&			size_class,
				const bool&			is_small,
				const bool&			is_sync,
				const gptr<gptr<block>>&	hp)
{
	if (BCL::rank() == host)
	{
		head_ptr = BCL::alloc<gptr<block>>(1);
		if (head_ptr == nullptr)
		{
			printf("[%lu]ERROR: pool_ubd_mpsc::pool_ubd_mpsc\n", BCL::rank());
			return;
		}

		gptr<char> dummy;
		if (is_small)	// the block is SMALL
			dummy = BCL::alloc<char>(size_class + 8);
		else	// the block is LARGE
			dummy = BCL::alloc<char>(size_class + 16);
		if (dummy == nullptr)
		{
			printf("[%lu]ERROR: pool_ubd_mpsc::pool_ubd_mpsc\n", BCL::rank());
			return;
		}

		if (is_small)	// the block is SMALL
		{
			bclx::store(size_class, {dummy.rank, dummy.ptr});	// local access
			head_addr = {dummy.rank, dummy.ptr + 8};
		}
		else	// the block is LARGE
		{
			gptr<header> ptr_hd = {dummy.rank, dummy.ptr};
			bclx::store({head_ptr, size_class}, ptr_hd);	// local access
			head_addr = {dummy.rank, dummy.ptr + 16};
		}
		bclx::store(head_addr, head_ptr);	// local access

		if (is_sync)
			head_ptr = BCL::broadcast(head_ptr, host);	// broadcast
		else { /* do nothing */ }
	}
	else // if (BCL::rank() != host)
	{
		if (is_sync)
			head_ptr = BCL::broadcast(head_ptr, host);	// broadcast
		else // if (!is_sync)
			head_ptr = hp;
	}
}

dds::pool_ubd_mpsc::~pool_ubd_mpsc() {}

bclx::gptr<bclx::gptr<bclx::block>> dds::pool_ubd_mpsc::get_head_ptr() const
{
	return head_ptr;
}

void dds::pool_ubd_mpsc::clear()
{
	/*if (BCL::rank() == HOST)
	{
		BCL::dealloc<block<T>>(dummy);
		BCL::dealloc<gptr<block>>(head_ptr);
	}*/
}

void dds::pool_ubd_mpsc::put(const gptr<void>& ptr)
{
        // Update the last remote global pointer and the head pointer of the pool
        gptr<gptr<block>> last = {ptr.rank, ptr.ptr};
        gptr<block> result;
        head_addr = {ptr.rank, ptr.ptr};
        gptr<block> curr = bclx::aget_sync(head_ptr);   // remote access
        while (true)
        {
                // Update the last remote global pointer
                bclx::rput_sync(curr, last);            // remote access

                // Update the head pointer of the pool
                result = bclx::cas_sync(head_ptr, curr, head_addr);     // remote access

                // Check if the cas has already succeeded
                if (result == curr)
                        break;
                else // if (result != curr)
                        curr = result;
        }
}

void dds::pool_ubd_mpsc::put(const std::vector<gptr<void>>& ptrs)
{
	// Prepare for the remote linked list
	std::vector<int64_t>	disp;
	std::vector<block>	buffer;
	block			tmp;
	for (uint64_t i = 0; i < ptrs.size() - 1; ++i)
	{
		disp.push_back(int64_t(ptrs[i].ptr) - int64_t(ptrs[0].ptr));
		tmp.next = {ptrs[i + 1].rank, ptrs[i + 1].ptr};
		buffer.push_back(tmp);
	}

	// Link the remote global pointers
	gptr<rll_t>	base = {ptrs[0].rank, ptrs[0].ptr};
	rll_t		rll(disp);
	bclx::rput_sync(buffer, base, rll);	// remote access
	
	// Update the last remote global pointer and the head pointer of the pool
	gptr<gptr<block>> last = {ptrs[ptrs.size() - 1].rank, ptrs[ptrs.size() - 1].ptr};
	gptr<block> result;
	head_addr = {base.rank, base.ptr};
	gptr<block> curr = bclx::aget_sync(head_ptr);	// remote access
	while (true)
	{
		// Update the last remote global pointer
		bclx::rput_sync(curr, last);		// remote access

		// Update the head pointer of the pool
		result = bclx::cas_sync(head_ptr, curr, head_addr);	// remote access

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
	gptr<block> curr = bclx::aget_sync(head_ptr);	// local access

	// Check if the head has been changed by the producer
	if (curr == head_addr)
		return false;	// the queue is empty now

	// Update the list
	slist.set(bclx::load(curr).next, head_addr);	// local access

	// Cache the head of the pool
	head_addr = curr;

	return true;
}

/**/

#endif /* POOL_UBD_MPSC_H */
