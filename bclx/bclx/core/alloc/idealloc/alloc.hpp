#pragma once

#include <string>				// std::string...
#include <cstdint>				// uint64_t...
#include <unordered_map>			// std::unordered_map...
#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>
#include <bclx/core/alloc/idealloc/heap.hpp>	// heap...

namespace bclx
{

//#define DEBUGGING

// debugging
#ifdef	DEBUGGING
	uint64_t cnt_ncontig	= 0;
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
  template <typename T>
	gptr<T> malloc(const uint64_t& size);
	void free(const gptr<void>& ptr);

private:
	const std::string	NAME		= "IdeAlloc";
	const uint64_t		HEADER_SIZE	= 8;
	heap			lheap;	// per-unit heap
}; /* class memory */

} /* namespace bclx */

const char* bclx::memory::get_name() const
{
	return NAME.c_str();
}

template <typename T>
bclx::gptr<T> bclx::memory::malloc(const uint64_t& size)
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

			gptr<void> ptr = scl.ncontig.back();
			scl.ncontig.pop_back();
			return {ptr.rank, ptr.ptr};
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

		// otherwise, get a batch of blocks from the BCL allocator
		gptr<char> batch = BCL::alloc<char>(BATCH_SIZE_MAX);
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
			gptr<uint64_t> ptr_sc = {ptr.rank, ptr.ptr};
			bclx::store(scl.size_class, ptr_sc);	// local access
		        return {ptr.rank, ptr.ptr + HEADER_SIZE};
		}

		// TODO: try to recycle every block in all scls
		
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
				gptr<void> ptr = scl->ncontig.back();
				scl->ncontig.pop_back();
				return {ptr.rank, ptr.ptr};
			}
		}

		// allocate a block with the corresponding size from the BCL allocator
		gptr<char> ptr = BCL::alloc<char>(size);
		if (ptr != nullptr)
		{
			// store the metadata of the block before passing it to the program
			gptr<uint64_t> ptr_hd = {ptr.rank, ptr.ptr};
			bclx::store(size, ptr_hd);    // local access
			return {ptr.rank, ptr.ptr + HEADER_SIZE};
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
			scl.ncontig.push_back(ptr);

			// TODO: if the size of @ncontig reaches some threshold, return a batch to the BCL allocator
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
			
			// move every block from @lheap.buffers to corresponding @scl.ncontig
			for (uint64_t i = 0; i < list_sc.size(); ++i)
			{
				// the requested size is SMALL
				if (list_sc[i] <= SIZE_CLASS_MAX)
				{
					// fetch the SCL corresponding with the size (class)
					scl_small& scl = lheap.small[list_sc[i] / DISTANCE - 1];

					// push the block into @ncontig
					scl.ncontig.push_back(ptr);
				}
				else // the requested size is LARGE
				{
					// check if the requested size has been processed before
					scl_large *scl;
					std::unordered_map<uint64_t, scl_large*>::const_iterator got =
										lheap.large.find(list_sc[i]);
					if (got == lheap.large.end())   // not found
					{
						// create a new SCL and then insert it into @large
						scl = new scl_large(list_sc[i]);
						if (scl == nullptr)
						{
							printf("[%lu]ERROR: memory.free_large\n", BCL::rank());
							return;
						}
						lheap.large.insert({list_sc[i], scl});
					}
					else // found
					{
						// fetch the SCL from @large
						scl = got->second;
					}

					// push the block into @ncontig
					scl->ncontig.push_back(ptr);
				}
			}

			// reset @lheap.buffers[ptr.rank]
			lheap.buffers[ptr.rank].clear();
		}

	}
}

/**/

