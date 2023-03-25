#pragma once

#include <cstdint>			// uint64_t...
#include <cmath>			// exp2l...
#include <vector>			// std::vector...
#include <unordered_map>		// std::unordered_map...
#include <bclx/core/alloc/list.hpp>	// list_seq...

namespace bclx
{

/* Macros and constants */
const uint64_t	SIZE_CLASS_MAX	= exp2l(12);	// 4 KB
const uint64_t	SIZE_CLASS_MIN	= 8;		// 8 B
const uint64_t	BATCH_SIZE_MAX	= exp2l(16);	// 64 KB
const uint64_t	DISTANCE	= 8;		// small size classes are 8 bytes apart
const uint64_t	FREQ_GET_RSIZE	= 1;		// the frequency of geting remote sizes

/* Interfaces */

class scl_small
{
public:
	list_seq			contig;		// the contig list
	std::vector<gptr<void>>		ncontig;	// the ncontig list
	uint64_t			size_class;	// the size class
	uint64_t			batch_num;	// the block count in a batch
										
	scl_small(const uint64_t& sc);
	~scl_small();
}; /* class scl_small */

class scl_large
{
public:
	std::vector<gptr<void>>		ncontig;	// the ncontig list
	
	scl_large(const uint64_t& size);
	~scl_large();
}; /* class scl_large */

class heap
{
public:
	std::vector<scl_small>				small;
	std::unordered_map<uint64_t, scl_large*>	large;
	std::vector<std::vector<gptr<void>>> 		buffers;
	
	heap();
	~heap();
}; /* class heap */

} /* namespace bclx */

/* Implementation of class scl_small */

bclx::scl_small::scl_small(const uint64_t& sc)
		: size_class{sc}
{
	uint64_t obj_size = size_class + 8;
	batch_num = BATCH_SIZE_MAX / obj_size;

	// initialize @contig
	contig.set_obj_size(obj_size);
}

bclx::scl_small::~scl_small() {}

/**/

/* Implementation of class scl_large */

bclx::scl_large::scl_large(const uint64_t& size) {}

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
