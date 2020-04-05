#ifndef COUNTER_NB2_H
#define COUNTER_NB2_H

namespace dds
{

namespace counter_nb2
{

	/* Data types */
	class counter
	{
	public:
		counter();
		~counter();
		uint64_t increment();

	private:
		const uint64_t	one = 1;
	
		gptr<uint64_t>	_count;
	};

} /* namespace counter_nb2 */

} /* namespace dds */

dds::counter_nb2::counter::counter()
{
	//synchronize
	BCL::barrier();

	_count = BCL::alloc<uint64_t>(1);
	BCL::store((uint64_t) 1, _count);
	_count.rank = (_count.rank + 1) % BCL::nprocs();

	if (BCL::rank() == MASTER_UNIT)
                printf("*\tCOUNTER\t\t:\tC_NB2\t\t\t*\n");

	//synchronize
	BCL::barrier();
}

dds::counter_nb2::counter::~counter()
{
	_count.rank = BCL::rank();
	BCL::dealloc<uint64_t>(_count);
}

uint64_t dds::counter_nb2::counter::increment()
{
	return BCL::fao_sync(_count, one, BCL::plus<uint64_t>{});
}

#endif /* COUNTER_NB2_H */
