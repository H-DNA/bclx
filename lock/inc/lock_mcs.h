#ifndef LOCK_MCS_H
#define LOCK_MCS_H

namespace dds
{

namespace mcsl
{

using namespace bclx;

struct elem
{
	gptr<elem>	next;
	bool		locked;
};

class lock
{
public:
	lock();
	~lock();
	void acquire();
	void release();

private:
	const gptr<elem>	NULL_PTR = nullptr;     //is a null constant

	gptr<gptr<elem>> 	tail;
	gptr<elem>		self;
};

} /* namespace mcsl */

} /* namespace dds */

dds::mcsl::lock::lock()
{
	//synchronize
	barrier_sync();

	self = BCL::alloc<elem>(1);
	tail = BCL::alloc<gptr<elem>>(1);
	tail.rank = MASTER_UNIT;

        //initialize value of tail (dummy node)
        if (BCL::rank() == MASTER_UNIT)
        	store(NULL_PTR, tail);

        //synchronize
	barrier_sync();
}

dds::mcsl::lock::~lock()
{
	tail.rank = BCL::rank();
        BCL::dealloc<gptr<elem>>(tail);
	BCL::dealloc<elem>(self);
}

void dds::mcsl::lock::acquire()
{
        gptr<elem> 		prevAddr;
	gptr<gptr<elem>>	nextAddr;
	gptr<bool>		lockedAddr;

	nextAddr = {self.rank, self.ptr};
	store(NULL_PTR, nextAddr);

	prevAddr = fao_sync(tail, self, BCL::replace<uint64_t>{});
	if (prevAddr != nullptr)	//queue was non-empty
	{
		lockedAddr = {self.rank, self.ptr + sizeof(gptr<elem>)};
		store(true, lockedAddr);

		nextAddr = {prevAddr.rank, prevAddr.ptr};
		aput_sync(self, nextAddr);

		while (aget_sync(lockedAddr));	//spin
	}
}

void dds::mcsl::lock::release()
{
	gptr<elem> 		result,
				nextVal;
        gptr<gptr<elem>>	nextAddr;
        gptr<bool>		lockedAddr;

	nextAddr = {self.rank, self.ptr};
	nextVal = aget_sync(nextAddr);

	if (nextVal == nullptr)	//no known successor
	{
		result = cas_sync(tail, self, NULL_PTR);
		if (result == self)
		{
			return;
			//compare_and_swap returns true iff it swapped
		}

		do {
			nextVal = aget_sync(nextAddr);
		} while (nextVal == nullptr);	//spin
	}

	lockedAddr = {nextVal.rank, nextVal.ptr + sizeof(gptr<elem>)};
	aput_sync(false, lockedAddr);
}

#endif /* LOCK_MCS_H */
