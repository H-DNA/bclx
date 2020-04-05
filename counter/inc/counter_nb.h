#ifndef COUNTER_NB_H
#define COUNTER_NB_H

namespace dds
{

namespace counter_nb
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

} /* namespace counter_nb */

} /* namespace dds */

dds::counter_nb::counter::counter()
{
	//synchronize
	BCL::barrier();

	_count = BCL::alloc<uint64_t>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		BCL::store((uint64_t) 1, _count);
                printf("*\tCOUNTER\t\t:\tC_NB\t\t\t*\n");
	}
	else //if (BCL::rank() != MASTER_UNIT)
		_count.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

dds::counter_nb::counter::~counter()
{
	if (BCL::rank() != MASTER_UNIT)
		_count.rank = BCL::rank();
	BCL::dealloc<uint64_t>(_count);
}

uint64_t dds::counter_nb::counter::increment()
{
	return BCL::fao_sync(_count, one, BCL::plus<uint64_t>{});
}

#endif /* COUNTER_NB_H */
