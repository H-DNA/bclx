#pragma once

#include <string>	// std::string...
#include <cstdint>	// uint64_t...
#include "heap.hpp"	// heap...

namespace bclx
{

/* Interface */
class memory
{
public:
	const char* get_name() const;
	gptr<void> malloc(const uint64_t& size);
	void free(const gptr<void>& ptr);

private:
	const std::string	NAME		= "HydrAlloc_2";
	const uint64_t		SCAN_FREQ	= 1;

	heap			lheap;	// per-unit heap
}; /* class memory */

} /* namespace bclx */

/* Implementation */

const char* bclx::memory::get_name() const
{
	return NAME.c_str();
}

bclx::gptr<void> bclx::memory::malloc(const uint64_t& size)
{
	if (size == 0)
		return nullptr;

	if (size <= SIZE_CLASS_MAX)	// the requested memory size is SMALL
	{
		// fetch the SCL corresponding with the size
		scl_small *tmp;
		uint64_t size_class = ((size - 1) / DISTANCE + 1) * DISTANCE;
		std::unordered_map<uint64_t, scl_small*>::const_iterator got = lheap.small.find(size_class);
		if (got == lheap.small.end())	// not found
		{
			tmp = new scl_small(size_class);
			if (tmp == nullptr)
			{
				printf("[%lu]ERROR: memory.malloc_small\n", BCL::rank());
				return nullptr;
			}
			lheap.small.insert({size_class, tmp});
		}
		else // found
			tmp = got->second;
		scl_small& scl = *tmp;

		// if scl.ncontig is not empty, return a gptr<void> from it
		if (!scl.ncontig.empty())
		{
			// tracing
			#ifdef  TRACING
				++elem_ru;
			#endif

			// debugging
			#ifdef  DEBUGGING
				++cnt_ncontig;
			#endif

			gptr<void> ptr = scl.ncontig.pop();
			return ptr;
		}

		// if scl.contig is not empty, return a gptr<void> from it
		if (!scl.contig.empty())
		{
			// debugging
			#ifdef	DEBUGGING
				++cnt_contig;
			#endif

			// pop a block
                        gptr<void> ptr = scl.contig.pop();

			// store the metadata of the block
                        gptr<header> ptr_header = {ptr.rank, ptr.ptr};
                        bclx::store({scl.pipe_recv->head_ptr, scl.size_class}, ptr_header);  // local access
                        return {ptr.rank, ptr.ptr + sizeof(header)};
		}


		// otherwise, scan the corresponding scl's pipe to reclaim remotely freed elems if any
		if (++scl.scan_count % SCAN_FREQ == 0)
		{
			list_seq2 slist;
			if (scl.pipe_recv->get(slist))			
				scl.ncontig.append(slist);
		
			// if scl.ncontig is not empty, return a gptr<void> from it
			if (!scl.ncontig.empty())
			{
				// tracing
				#ifdef	TRACING
					++elem_ru;
				#endif

				// debugging
				#ifdef	DEBUGGING
					++cnt_ncontig2;
				#endif

				gptr<void> ptr = scl.ncontig.pop();
				return ptr;
			}
		}

		// otherwise, get blocks from the BCL allocator
		gptr<char> batch = BCL::alloc<char>(scl.batch_size);
		if (batch != nullptr)
		{
			// debugging
			#ifdef	DEBUGGING
				++cnt_pool;
			#endif

			// insert the batch into contig
			scl.contig.push(batch, scl.batch_num);

			// pop a block 
			gptr<void> ptr = scl.contig.pop();

			// store the metadata of the block
			gptr<header> ptr_header = {ptr.rank, ptr.ptr};
			bclx::store({scl.pipe_recv->head_ptr, scl.size_class}, ptr_header);	// local access
		        return {ptr.rank, ptr.ptr + sizeof(header)};
		}

		// try to scan all pipes to reclaim remotely freed elems one more time
		/*for (uint64_t i = 0; i < scl.pipes[BCL::rank()].size(); ++i)
		{
			list_seq2 slist;
			if (scl.pipes[BCL::rank()][i].get(slist))
				scl.ncontig.append(slist);
		}

		// if scl.ncontig is not empty, return a gptr<T> from it
		if (!scl.ncontig.empty())
		{
			// tracing
			#ifdef	TRACING
				++elem_ru;
			#endif

		        gptr<void> ptr = scl.ncontig.pop();
		        return ptr;
		}*/
		
		// otherwise, return nullptr
		// TODO
		printf("[%lu]ERROR: memory.malloc_small\n", BCL::rank());
		return nullptr;
	}
	else // the requested memory size is LARGE
	{
		// TODO
		printf("[%lu]ERROR: memory.malloc_large\n", BCL::rank());
		return nullptr;
	}
}

void bclx::memory::free(const gptr<void>& ptr)
{
	// debugging
	#ifdef	DEBUGGING
		++cnt_free;
	#endif

	if (ptr == nullptr)
		return;

	if (ptr.rank == BCL::rank())	// the deallocation is LOCAL
	{
		// get the size class of the block
		gptr<uint64_t> ptr_size = {ptr.rank, ptr.ptr - 8};
		uint64_t size_class = bclx::load(ptr_size);	// local access
					
		if (size_class <= SIZE_CLASS_MAX)	// the requested size is SMALL
		{
			// fetch the SCL corresponding with the size class
			scl_small& scl = *lheap.small[size_class];

			scl.ncontig.push(ptr);
			/*if (scl.ncontig.size() >= 2 * scl.batch_num)
			{
				gptr<void> tmp;
				for (uint64_t i = 0; i < scl.ncontig.size() / 2; ++i)
				{
					tmp = scl.ncontig.pop();
					BCL::dealloc<void>(tmp);
				}
			}*/
		}
		else // the requested size is LARGE
		{
			// TODO
			printf("[%lu]ERROR: memory.free_local_large\n", BCL::rank());
			return;
		}
	}
	else // the deallocation is REMOTE
	{
		lheap.buffers[ptr.rank].push_back(ptr);
		if (lheap.buffers[ptr.rank].size() >= 4096)
		{
			// compute the list of displacements
			std::vector<int64_t> disp;
			for (uint64_t i = 0; i < lheap.buffers[ptr.rank].size(); ++i)
				disp.push_back(int64_t(lheap.buffers[ptr.rank][i].ptr) -
						int64_t(lheap.buffers[ptr.rank][0].ptr));

			// get the batch of the blocks' headers
			std::vector<header>	list_hd;
        		gptr<rll_t>     	base = {lheap.buffers[ptr.rank][0].rank,
							lheap.buffers[ptr.rank][0].ptr - sizeof(header)};
        		rll_t           	rll(disp);
        		bclx::rget_sync(base, list_hd, rll);     // remote access
								 
			// move every block from list_hd to corresponding buffers
			for (uint64_t i = 0; i < list_hd.size(); ++i)
			{
				// the requested size is SMALL
				if (list_hd[i].size_class <= SIZE_CLASS_MAX)
				{
                        		// fetch the SCL corresponding with the size
                        		scl_small& scl = *lheap.small[list_hd[i].size_class];

					scl.buffers[ptr.rank].push_back(lheap.buffers[ptr.rank][i]);
					if (scl.buffers[ptr.rank].size() >= scl.batch_num)
					{
						// send the batch of blocks to the target process
						dds::pool_ubd_mpsc pipe_send(list_hd[i].head_ptr);
						pipe_send.put(scl.buffers[ptr.rank]);

						// reset scl.buffers[ptr.rank]
						scl.buffers[ptr.rank].clear();
					}
				}
				else // the requested size is LARGE
				{
					// TODO
					printf("[%lu]ERROR: memory.free_remote_large\n", BCL::rank());
					return;
				}
			}

			// reset lheap.buffers[ptr.rank]
			lheap.buffers[ptr.rank].clear();
		}
	}
}

/**/

