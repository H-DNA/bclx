#ifndef LOCK_TAS_H
#define LOCK_TAS_H

#include "lock_inf.h"

namespace dds
{

class lock_tas : public lock /* Test And Set Lock */
{
public:
        lock_tas();
        ~lock_tas();
        void acquire() override;
        void release() override;

private:
	const int MASTER_RANK = 0;
        BCL::GlobalPtr<bool>     flag;

}; /* class lock_tas */

} /* namespace dds */

dds::lock_tas::lock_tas()
{
	// synchronize
	BCL::barrier();

	// allocate global (or shared) memory for flag
	flag = BCL::alloc<bool>(1);

	// initialize flag with 0
	if (BCL::rank() == MASTER_RANK)
		BCL::store(false, flag);
	else // if (BCL::rank() != MASTER_RANK)
		flag.rank = MASTER_RANK;

	// synchronize
	BCL::barrier();
}

dds::lock_tas::~lock_tas()
{
	// deallocate global memory of flag
	if (BCL::rank() != MASTER_RANK)
		flag.rank == BCL::rank();
        BCL::dealloc<bool>(flag);
}

void dds::lock_tas::acquire()
{
	while (BCL::fao_sync(flag, true, BCL::replace<bool>{}));
}

void dds::lock_tas::release()
{
	BCL::aput_sync(false, flag);
}

#endif /* LOCK_TAS_H */

