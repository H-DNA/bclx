#pragma once

#include <cstdint>					// uint64_t...
#include <cmath>					// exp2l...
#include <vector>					// std::vector...
#include <unordered_map>				// std::unordered_map...
#include <bclx/core/hydralloc/list.hpp>			// list_seq...
#include "../../../../../pool/inc/pool_ubd_spsc.h"	// pool_ubd_spsc...

namespace bclx
{

/* Macros and constants */
const uint64_t	SIZE_CLASS_MAX	= exp2l(12);	// 4 KB
const uint64_t	SIZE_CLASS_MIN	= 8;		// 8 B
const uint64_t	BATCH_SIZE_MAX	= exp2l(16);	// 64 KB
const uint64_t	DISTANCE	= 8;		// small size classes are 8 bytes apart

/* Interfaces */

class scl_small
{
public:
	list_seq					contig;		// the contig list
	list_seq3					ncontig;	// the ncontig list
	std::vector<std::vector<gptr<void>>>		buffers;	// local buffers
	std::vector<std::vector<dds::pool_ubd_spsc>>	pipes;		// SPSC unbounded pools
	uint64_t					size_class;	// the size class
	uint64_t					batch_num;	// the block count in a batch
	uint64_t					batch_size;	// the actual batch size
	uint64_t					scan_count;	// the number of times the hosted pipes
									// have not yet been scanned
										
	scl_small(const uint64_t& sc);
	~scl_small();
}; /* class scl_small */

class scl_large
{
public:
	list_seq2					ncontig;	// the ncontig list
	std::vector<dds::pool_ubd_spsc>			pipes;		// SPSC unbounded pools
	
	scl_large(const uint64_t& size);
	~scl_large();
}; /* class scl_large */

class heap
{
public:
	std::vector<scl_small>				small;
	std::unordered_map<uint64_t, scl_large*>	large;	// TODO
	std::vector<std::vector<gptr<void>>> 		buffers;
	
	heap();
	~heap();
}; /* class heap */

} /* namespace bclx */

/* Implementation of class scl_small */

bclx::scl_small::scl_small(const uint64_t& sc)
		: size_class{sc}, scan_count{0}
{
	uint64_t obj_size = size_class + 8;
	batch_num = BATCH_SIZE_MAX / obj_size;
	batch_size = batch_num * obj_size;

	// initialize @contig
	contig.set_obj_size(obj_size);

	// initialize @ncontig
	ncontig.set_batch_num(batch_num);

	// initialize @buffers & @pipes
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
	{
		buffers.push_back(std::vector<gptr<void>>());
		
		pipes.push_back(std::vector<dds::pool_ubd_spsc>());
		for (uint64_t j = 0; j < BCL::nprocs(); ++j)
			pipes[i].push_back(dds::pool_ubd_spsc(i, obj_size, true, true, nullptr));
	}
}

bclx::scl_small::~scl_small() {}

/**/

/* Implementation of class scl_large */

bclx::scl_large::scl_large(const uint64_t& size)
{
	// initialize @pipes
	for (uint64_t i = 0; i < BCL::nprocs(); ++i)
		pipes.push_back(dds::pool_ubd_spsc(i, size + 16, false, false, nullptr));
}

bclx::scl_large::~scl_large() {}

/**/

/* Implementation of class heap */

bclx::heap::heap()
{
	// initialize @small
	uint64_t size_class;
	for (uint64_t i = 0; i < (SIZE_CLASS_MAX - SIZE_CLASS_MIN) / DISTANCE + 1; ++i)
	{
		size_class = DISTANCE * (i + 1);
		small.push_back(scl_small(size_class));
	}

        // initialize @buffers
        for (uint64_t i = 0; i < BCL::nprocs(); ++i)
                buffers.push_back(std::vector<gptr<void>>());
}

bclx::heap::~heap() {}

/**/
