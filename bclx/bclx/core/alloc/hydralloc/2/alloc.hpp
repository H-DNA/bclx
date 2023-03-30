#pragma once

#include <string>				// std::string...
#include <cstdint>				// uint64_t...
#include <unordered_map>			// std::unordered_map...
#include <bclx/core/alloc/hydralloc/2/heap.hpp>	// heap...

namespace bclx
{

//#define	DEBUGGING

// debugging
#ifdef	DEBUGGING
	uint64_t cnt_ncontig	= 0;
	uint64_t cnt_ncontig2	= 0;
	uint64_t cnt_contig	= 0;
	uint64_t cnt_bcl	= 0;
	uint64_t cnt_free	= 0;
	uint64_t cnt_lfree	= 0;
	uint64_t cnt_rfree	= 0;
#endif

/* Interface */

class memory
{
public:
	const char* get_name() const;
	gptr<void> malloc(const uint64_t& size);
	void free(const gptr<void>& ptr);

private:
	const std::string	NAME		= "HydrAlloc_2";
	const uint64_t		HEADER_SIZE	= 8;
	heap			lheap;	// per-unit heap
}; /* class memory */

} /* namespace bclx */

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
		scl_small& scl = lheap.small[(size - 1) / DISTANCE];

		// if @ncontig is not empty, allocate a block from it
		if (!scl.ncontig.empty())
		{
			// debugging
			#ifdef  DEBUGGING
				++cnt_ncontig;
			#endif

			gptr<void> ptr = scl.ncontig.pop();
			return ptr;
		}

		// if @contig is not empty, allocate a block from it
		if (!scl.contig.empty())
		{
			// debugging
			#ifdef	DEBUGGING
				++cnt_contig;
			#endif

                        gptr<void> ptr = scl.contig.pop();
                        gptr<uint64_t> ptr_sc = {ptr.rank, ptr.ptr};
                        bclx::store(scl.size_class, ptr_sc);  // local access
                        return {ptr.rank, ptr.ptr + HEADER_SIZE};
		}

		// otherwise, try to scan all pipes of the current SCL to reclaim remotely freed blocks (if any)
		if (++scl.scan_count % FREQ_SCAN_PIPE == 0)
		{
			list_seq2 slist;
			if (scl.pipes[BCL::rank()].get(slist))			
				scl.ncontig.push(slist);
		
			// if @ncontig is not empty, allocate a block from it
			if (!scl.ncontig.empty())
			{
				// debugging
				#ifdef	DEBUGGING
					++cnt_ncontig2;
				#endif

				gptr<void> ptr = scl.ncontig.pop();
				return ptr;
			}
		}

		// otherwise, get a batch of blocks from the BCL allocator
		gptr<char> batch = BCL::alloc<char>(scl.batch_size);
		if (batch != nullptr)
		{
			// debugging
			#ifdef	DEBUGGING
				++cnt_bcl;
			#endif

			// push the batch into @contig
			scl.contig.push(batch, scl.batch_num);

			// allocate a block from @contig
			gptr<void> ptr = scl.contig.pop();

			// store the metadata of the block before passing it to the program
			gptr<uint64_t> ptr_sc = {ptr.rank, ptr.ptr};
			bclx::store(scl.size_class, ptr_sc);	// local access
		        return {ptr.rank, ptr.ptr + HEADER_SIZE};
		}

		// try to scan all pipes of every SCL to reclaim remotely freed blocks (if any)
		// TODO
		/*for (uint64_t i = 0; i < lheap.small.size(); ++i)
		{
			list_seq2 slist;
			if (lheap.small[i].pipes[BCL::rank()].get(slist))
				scl.ncontig.push(slist);

			// if scl.ncontig is not empty, return a gptr<void> from it
			if (!lheap.small[i].ncontig.empty())
			{
				// tracing
				#ifdef	TRACING
				++elem_ru;
				#endif

		        	gptr<void> ptr = lheap.small[i].ncontig.pop();
		        	return ptr;
			}
		}*/
		
		// otherwise, return a null pointer
		printf("[%lu]ERROR: memory.malloc_small\n", BCL::rank());
		return nullptr;
	}
	else // the requested memory size is LARGE
	{
		// check if the requested size has been processed before
		scl_large *scl;
		std::unordered_map<uint64_t, scl_large*>::const_iterator got = lheap.large.find(size);
		if (got == lheap.large.end())	// not found
		{
			// create a new SCL and then insert it into @large
			scl = new scl_large(size);
			if (scl == nullptr)
			{
                                printf("[%lu]ERROR: memory.malloc_large\n", BCL::rank());
                                return nullptr;
			}
			lheap.large.insert({size, scl}); 
		}
		else // found
		{
			// fetch the SCL from @large
			scl = got->second;

			// if @ncontig is not empty, allocate a block from it
			if (!scl->ncontig.empty())
			{
				gptr<void> ptr = scl->ncontig.pop();
				return ptr;
			}

			// scan all the @pipes
                        for (uint64_t i = 0; i < scl->pipes.size(); ++i)
                        {
                                list_seq2 slist;
                                if (scl->pipes[i].get(slist))
                                        scl->ncontig.push(slist);
                        }		

			// if @ncontig is not empty, allocate a block from it
			if (!scl->ncontig.empty())
			{
                                gptr<void> ptr = scl->ncontig.pop();
                                return ptr;
                        }	
		}

		// allocate a block with the corresponding size from the BCL allocator
		gptr<char> ptr = BCL::alloc<char>(size);
		if (ptr != nullptr)
		{
			// store the metadata of the block before passing it to the program
			gptr<header> ptr_hd = {ptr.rank, ptr.ptr};
			bclx::store({scl->pipes[0].get_head_ptr(), size}, ptr_hd);    // local access
			return {ptr.rank, ptr.ptr + sizeof(header)};
		}

		// otherwise, return a null pointer
		printf("[%lu]ERROR: memory.malloc_large\n", BCL::rank());
		return nullptr;
	}
}

void bclx::memory::free(const gptr<void>& ptr)
{
	if (ptr == nullptr)
		return;

	if (ptr.rank == BCL::rank())	// the deallocation is LOCAL
	{
		// debugging
		#ifdef	DEBUGGING
			++cnt_lfree;
		#endif

		// get the size of the block
		gptr<uint64_t> ptr_size = {ptr.rank, ptr.ptr - HEADER_SIZE};
		uint64_t size = bclx::load(ptr_size);	// local access
					
		if (size <= SIZE_CLASS_MAX)	// the requested size is SMALL
		{
			// fetch the SCL corresponding with the size (class)
			scl_small& scl = lheap.small[size / DISTANCE - 1];

			// push the block into @ncontig
			scl.ncontig.push(ptr);

			// check if the size of @ncontig reaches some predefined threshold
			// TODO
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
			// directly free the block using the BCL allocator
			BCL::dealloc<void>(ptr);
		}
	}
	else // the deallocation is REMOTE
	{
		// debugging
		#ifdef	DEBUGGING
			++cnt_rfree;
		#endif

		lheap.buffers[ptr.rank].push_back(ptr);
		if (lheap.buffers[ptr.rank].size() >= FREQ_GET_RSIZE)
		{
			// compute the list of displacements
			std::vector<int64_t> disp;
			for (uint64_t i = 0; i < lheap.buffers[ptr.rank].size(); ++i)
				disp.push_back(int64_t(lheap.buffers[ptr.rank][i].ptr) -
						int64_t(lheap.buffers[ptr.rank][0].ptr));

			// get the batch of the blocks' size classes
			std::vector<uint64_t>	list_sc(FREQ_GET_RSIZE);
			gptr<rll_t>		base = {lheap.buffers[ptr.rank][0].rank,
							lheap.buffers[ptr.rank][0].ptr - HEADER_SIZE};
			rll_t			rll(disp);
			bclx::rget_sync(base, list_sc, rll);	// remote access
			
			// move every block from @lheap.buffers to corresponding @scl.buffers
			for (uint64_t i = 0; i < list_sc.size(); ++i)
			{
				// the requested size is SMALL
				if (list_sc[i] <= SIZE_CLASS_MAX)
				{
					// fetch the SCL corresponding with the size (class)
					scl_small& scl = lheap.small[list_sc[i] / DISTANCE - 1];

					// move the block to the buffer corresponding with the size (class)
					scl.buffers[ptr.rank].push_back(lheap.buffers[ptr.rank][i]);
					if (scl.buffers[ptr.rank].size() >= scl.batch_num)
					{
						// send the batch of blocks to the target process
						scl.pipes[ptr.rank].put(scl.buffers[ptr.rank]);
					
						// reset scl.buffers[ptr.rank]
						scl.buffers[ptr.rank].clear();
					}
				}
				else // the requested size is LARGE
				{
					// determine the global access of the receiving pipe
					gptr<gptr<gptr<block>>> ptr_pipe = {lheap.buffers[ptr.rank][i].rank,
									lheap.buffers[ptr.rank][i].ptr - sizeof(header)};
					gptr<gptr<block>> pipe_head = rget_sync(ptr_pipe);	// remote access
					pipe_head.ptr += BCL::rank() * sizeof(gptr<void>);

					// initialize the sending pipe
					dds::pool_ubd_mpsc pipe_recv(ptr.rank, 0, false, false, pipe_head);

					// send the block to the target process
					pipe_recv.put(lheap.buffers[ptr.rank][i]);
				}
			}

			// reset @lheap.buffers[ptr.rank]
			lheap.buffers[ptr.rank].clear();
		}

	}
}

/**/

