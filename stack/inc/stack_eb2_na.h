#ifndef STACK_EB2_NA_H
#define STACK_EB2_NA_H

#include <random>			// std::uniform_int_distribution...
#include <bclx/core/util/backoff.hpp>	// backoff::backoff...
#include <bclx/core/util/ta.hpp>	// ta::na...

namespace dds
{

namespace ebs2_na
{

/* Macros */
#ifdef		MEM_HP
	using namespace hp;
#elif defined	MEM_EBR
	using namespace ebr;
#elif defined	MEM_EBR2
	using namespace ebr2;
#elif defined 	MEM_EBR3
	using namespace ebr3;
#elif defined 	MEM_HE
	using namespace he;
#elif defined	MEM_IBR
	using namespace ibr;
#elif defined	MEM_DANG
	using namespace dang;
#elif defined 	MEM_DANG2
	using namespace dang2;
#elif defined	MEM_DANG3
	using namespace dang3;
#elif defined	MEM_DANG4
	using namespace dang4;
#elif defined	MEM_DANG5
	using namespace dang5;
#elif defined	MEM_DANG6
	using namespace dang6;
#else	// No Memory Reclamation
	using namespace nmr;
#endif

/* Data types */
struct adapt_params
{
	uint32_t 	count;
	float 		factor;
};

template<typename T>
struct elem
{
	gptr<elem<T>>	next;
	T		value;
};

template<typename T>
struct unit_info
{
	gptr<elem<T>>	itsElem;
	uint32_t	rank;
	char		op;
};

template<typename T>
class stack
{
public:
	stack();			// collective
	stack(const uint64_t &num);	// collective
	~stack();			// collective
	bool push(const T &value);	// non-collective
	bool pop(T &value);		// non-collective
	void print();			// collective

private:
       	const gptr<elem<T>> 		NULL_PTR_E	= nullptr;
	const gptr<unit_info<T>>	NULL_PTR_U	= nullptr;
        const uint32_t       		NULL_UNIT	= -1;
        const uint32_t       		COUNT_INIT	= 5;
	const uint32_t			COUNT_MAX	= 2 * COUNT_INIT;
	const float			FACTOR_INIT	= 0.5;
        const float     		FACTOR_MIN	= 0.0;
        const float     		FACTOR_MAX	= 1.0;
        #define         		COLL_SIZE	BCL::nprocs()
	const bool			PUSH		= true;
	const bool			POP		= false;
	const bool			SHRINK		= true;
	const bool			ENLARGE		= false;

	memory<elem<T>>			mem;		// contain essential stuffs to manage globmem of elems
	memory<unit_info<T>>		mem2;		// manage global memory allocated for unit_info<T>(s)
	gptr<gptr<elem<T>>>		top;		// contain global memory address of the dummy node
	gptr<gptr<unit_info<T>>>	location;	// contain global mem add of a gptr of a @unit_info
	gptr<unit_info<T>>		p;
	gptr<uint32_t>			collision;	// contain the rank of the unit trying to collide
	adapt_params			adapt;		// contain the adaptative elimination backoff
	ta::na                          na;		// contain node information

	bool push_fill(const T &value);
	uint32_t get_position();
	void adapt_width(const bool &);
	bool try_collision(const gptr<unit_info<T>> &, const uint32_t &);
	void finish_collision();
	bool try_perform_stack_op();
	void less_op();
};

} /* namespace ebs2_na */

} /* namespace dds */

template<typename T>
dds::ebs2_na::stack<T>::stack()
{
        // synchronize
	BCL::barrier();

        location = BCL::alloc<gptr<unit_info<T>>>(1);
        BCL::store(NULL_PTR_U, location);

        collision = BCL::alloc<uint32_t>(ceil(COLL_SIZE / BCL::nprocs()));
        BCL::store(NULL_UNIT, collision);

        adapt = {COUNT_INIT, FACTOR_INIT};

        top = BCL::alloc<gptr<elem<T>>>(1);
        if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR_E, top);
		stack_name = "EBS2_NA";
	}
	else // if (BCL::rank() != MASTER_UNIT)
		top.rank = MASTER_UNIT;

	// synchronize
	BCL::barrier();
}

template<typename T>
dds::ebs2_na::stack<T>::stack(const uint64_t &num)
{
	// synchronize
	BCL::barrier();

	location = BCL::alloc<gptr<unit_info<T>>>(1);
	BCL::store(NULL_PTR_U, location);

	collision = BCL::alloc<uint32_t>(ceil(COLL_SIZE / BCL::nprocs()));
	BCL::store(NULL_UNIT, collision);

	adapt = {COUNT_INIT, FACTOR_INIT};

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		BCL::store(NULL_PTR_E, top);
		stack_name = "EBS2_NA";

		for (uint64_t i = 0; i < num; ++i)
		push_fill(i);
	}
	else // if (BCL::rank() != MASTER_UNIT)
		top.rank = MASTER_UNIT;

	// synchronize
	BCL::barrier();
}

template<typename T>
dds::ebs2_na::stack<T>::~stack()
{
	top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);

	collision.rank = BCL::rank();
        BCL::dealloc<uint32_t>(collision);

	location.rank = BCL::rank();
	BCL::dealloc<gptr<unit_info<T>>>(location);
}

template<typename T>
bool dds::ebs2_na::stack<T>::push(const T &value)
{
	unit_info<T> 	temp;
	gptr<T>		tempAddr;

	temp.rank = BCL::rank();
	temp.op = PUSH;
	// allocate global memory to the new elem
	temp.itsElem = mem.malloc();
	p = mem2.malloc();
	if (p == NULL_PTR_U || temp.itsElem == NULL_PTR_E)
	{
                printf("The stack is FULL\n");
		return false;
	}

	tempAddr = {temp.itsElem.rank, temp.itsElem.ptr + sizeof(gptr<elem<T>>)};
	#ifdef	MEM_HP
		BCL::rput_sync(value, tempAddr);
	#else	// MEM_NMR, MEM_DANG3

		// debugging
		// printf("[%lu]ALLOC: <%u,%u>\n", BCL::rank(), tempAddr.rank, tempAddr.ptr);

		BCL::store(value, tempAddr);
	#endif
	BCL::store(temp, p);

	less_op();

	return true;
}

template<typename T>
bool dds::ebs2_na::stack<T>::pop(T &value)
{
	unit_info<T> temp;

	temp.rank = BCL::rank();
	temp.op = POP;
	// allocate global memory to the new elem
	p = mem2.malloc();
	if (p == NULL_PTR_U)
	{
		printf("The stack is FULL\n");
		return false;
	}
	BCL::store(temp, p);

	less_op();

	gptr<gptr<elem<T>>> tempAddr = {p.rank, p.ptr};
	gptr<elem<T>> tempAddr2 = BCL::load(tempAddr);

	if (tempAddr2 == NULL_PTR_E)
	{
		mem2.free(p);
		return false;
	}
	else
	{
		gptr<T>	tempAddr3 = {tempAddr2.rank, tempAddr2.ptr + sizeof(gptr<elem<T>>)};
		value = BCL::rget_sync(tempAddr3);

                // deallocate global memory of the popped elem
                mem.free(tempAddr2);

		return true;
	}
}

template<typename T>
void dds::ebs2_na::stack<T>::print()
{
	// synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
                gptr<elem<T>> 	topAddr;
                elem<T>		topVal;

                for (topAddr = BCL::load(top); topAddr != nullptr; topAddr = topVal.next)
                {
                        topVal = BCL::rget_sync(topAddr);
                        printf("value = %d\n", topVal.value);
                        topVal.next.print();
                }
	}

	// synchronize
	BCL::barrier();
}

template<typename T>
bool dds::ebs2_na::stack<T>::push_fill(const T &value)
{
        if (BCL::rank() == MASTER_UNIT)
        {
                gptr<elem<T>>   tempAddr;
                gptr<elem<T>>   oldTopAddr;

                // allocate global memory to the new elem
                tempAddr = mem.malloc();
                if (tempAddr == nullptr)
                {
                        printf("The stack is FULL\n");
                        return false;
                }

                // get top (from global memory to local memory)
                oldTopAddr = BCL::load(top);

                // update new element (global memory)
                BCL::store({oldTopAddr, value}, tempAddr);

                // update top (global memory)
                BCL::store(tempAddr, top);
        }

	return true;
}

template<typename T>
bool dds::ebs2_na::stack<T>::try_perform_stack_op()
{
	unit_info<T> pVal = BCL::load(p);

	// debugging
	// printf("[%lu]<%u,%u>\n", BCL::rank(), p.rank, p.ptr);

	if (pVal.op == PUSH)
	{
		// get top
		gptr<elem<T>> oldTopAddr = BCL::aget_sync(top);

		// try to reserve top
		if (!mem.try_reserve(top, oldTopAddr))
			return false;

		// update new element (global memory)
        	gptr<gptr<elem<T>>> tempAddr = {pVal.itsElem.rank, pVal.itsElem.ptr};
		#ifdef	MEM_HP
			BCL::rput_sync(oldTopAddr, tempAddr);
		#else	// MEM_NMR, MEM_DANG3

			// debugging
			// printf("[%lu]PUSH: <%u,%u>\n", BCL::rank(), tempAddr.rank, tempAddr.ptr);

			BCL::store(oldTopAddr, tempAddr);
		#endif

		// try to update top
		gptr<elem<T>> result = BCL::cas_sync(top, oldTopAddr, pVal.itsElem);

		// unreserve top
		mem.unreserve(oldTopAddr);

		// check if the update is successful
		if (result == oldTopAddr)
		{
			mem2.free(p);
			return true;
		}
		else // if (result != oldTopAddr)
			return false;
	}
	else // if (pVal.op == POP)
	{
                gptr<gptr<elem<T>>> tempAddr = {p.rank, p.ptr};

		// get top
		gptr<elem<T>> oldTopAddr = BCL::aget_sync(top);

		// check if the stack is empty
		if (oldTopAddr == nullptr)
		{
			BCL::store(NULL_PTR_E, tempAddr);
			return true;
		}

		// try to reserve top
		if (!mem.try_reserve(top, oldTopAddr))
			return false;

		// get node (from global memory to local memory)
		elem<T> newTopVal = BCL::rget_sync(oldTopAddr);

		// try to update top
		gptr<elem<T>> result = BCL::cas_sync(top, oldTopAddr, newTopVal.next);

		// unreserve top
		mem.unreserve(oldTopAddr);

		// check if the update is successful
		if (result == oldTopAddr)
		{
			BCL::store(oldTopAddr, tempAddr);
			return true;
		}
		else // if (result != oldTopAddr)
			return false;
	}
}

template<typename T>
uint32_t dds::ebs2_na::stack<T>::get_position()
{
	uint32_t 	min, max;

	if (na.size % 2 == 0)
		min = round((na.size / 2 - 1) * (FACTOR_MAX - FACTOR_MIN - adapt.factor));
	else // if (na.size % 2 != 0)
		min = round(na.size / 2 * (FACTOR_MAX - FACTOR_MIN - adapt.factor));
	max = round(na.size / 2 * (FACTOR_MAX - FACTOR_MIN + adapt.factor));

	std::default_random_engine generator;
	std::uniform_int_distribution<uint32_t> distribution(min, max);
	return na.table[distribution(generator)];	// generate number in the range min..max
}

template<typename T>
void dds::ebs2_na::stack<T>::adapt_width(const bool &dir)
{
	if (dir == SHRINK)
	{
		if (adapt.count > 0)
			--adapt.count;
		else // if (adapt.count == 0)
		{
			adapt.count = COUNT_INIT;
			adapt.factor = std::max(adapt.factor / 2, FACTOR_MIN);
		}
	}
	else // if (dir == ENLARGE)
	{
		if (adapt.count < COUNT_MAX)
			++adapt.count;
		else // if (adapt.count == MAX_COUNT)
		{
			adapt.count = COUNT_INIT;
			adapt.factor = std::min(2 * adapt.factor, FACTOR_MAX);
		}
	}
}

template<typename T>
bool dds::ebs2_na::stack<T>::try_collision(const gptr<unit_info<T>> &q, const uint32_t &him)
{
	gptr<unit_info<T>>	result;
	unit_info<T>		pVal = BCL::load(p);

	location.rank = him;
	if (pVal.op == PUSH)
	{
		// attempt to eliminate @p and @q
		result = BCL::cas_sync(location, q, p);

		// check if any collision occured 
		if (result == q)
			return true;
		else // if (result != q)
		{
			adapt_width(ENLARGE);
			return false;
		}
	}
	else // if (pVal.op == POP)
	{
		// attempt to eliminate @p and @q
		result = BCL::cas_sync(location, q, NULL_PTR_U);

		// check if any collision occured
		if (result == q)
		{
			gptr<elem<T>> tempVal = BCL::rget_sync((gptr<gptr<elem<T>>>) {q.rank, q.ptr});
			BCL::store(tempVal, (gptr<gptr<elem<T>>>) {p.rank, p.ptr});

			return true;
		}
		else // if (result != q)
		{
			adapt_width(ENLARGE);
			return false;
		}
	}
}

template<typename T>
void dds::ebs2_na::stack<T>::finish_collision()
{
	unit_info<T> pVal = BCL::load(p);
	if (pVal.op == POP)
	{
		location.rank = BCL::rank();
                gptr<elem<T>> tempVal = BCL::aget_sync((gptr<gptr<elem<T>>>) {location.rank, location.ptr});
                BCL::store(tempVal, (gptr<gptr<elem<T>>>) {p.rank, p.ptr});
		BCL::aput_sync(NULL_PTR_U, location);;
	}
}

template<typename T>
void dds::ebs2_na::stack<T>::less_op()
{
	uint32_t		myUID = BCL::rank(),
				pos,
				him;
	unit_info<T>		pVal,
				qVal;
	gptr<unit_info<T>>	q,
				result;
	backoff::backoff	bk(bk_init, bk_max);

	// tracing
	#ifdef	TRACING
		double		start;
	#endif

	while (true)
	{
		// tracing
		#ifdef	TRACING
			start = MPI_Wtime();
		#endif

		// publish my @p
		location.rank = myUID;
		BCL::aput_sync(p, location);

		// get the index of the target collision entry
		pos = get_position();

		// swap the target collision entry with my unit rank
		collision.rank = pos;
		do {
			him = BCL::aget_sync(collision);
		} while (BCL::cas_sync(collision, him, myUID) != him);

		if (him != NULL_UNIT)
		{
			// get @q of the target unit
			location.rank = him;
			q = BCL::aget_sync(location);

			// try to reserve location[him]
			if (q != NULL_PTR_U && !mem2.try_reserve(location, q))
                        	goto label;

			if (q != NULL_PTR_U)
			{
				qVal = BCL::rget_sync(q);
				pVal = BCL::load(p);
				if (qVal.rank == him && pVal.op != qVal.op)
				{
					location.rank = myUID;
					if (BCL::cas_sync(location, p, NULL_PTR_U) == p)
					{
						if (try_collision(q, him))
						{
							// tracing
							#ifdef	TRACING
								++succ_ea;
							#endif

							// unreserve @q
							mem2.unreserve(q);

							return;
						}
						else
						{
							// unreserve @q
							mem2.unreserve(q);

							// debugging
							printf("[%lu]GOTO\n", BCL::rank());

							goto label;
						}
					}
					else
					{	// unreserve @q
						mem2.unreserve(q);

						finish_collision();

						// tracing
						#ifdef	TRACING
							++succ_ea;
						#endif

						return;
					}
				}
				else
				{
					// unreserve @q
					mem2.unreserve(q);

					// debugging
					printf("[%lu]BRANCH3\n", BCL::rank());
				}
			}
			else
			{
				// debugging
				printf("[%lu]BRANCH2\n", BCL::rank());
			}
		}
		else
		{
			// debugging
			printf("[%lu]BRANCH1\n", BCL::rank());
		}

		bk.delay_dbl();
		adapt_width(SHRINK);

		location.rank = myUID;
		if (BCL::cas_sync(location, p, NULL_PTR_U) != p)
		{
			finish_collision();

			// tracing
			#ifdef	TRACING
				++succ_ea;
			#endif

			return;
		}

	label:
		// tracing
		#ifdef	TRACING
			fail_time += (MPI_Wtime() - start);
			++fail_ea;
			start = MPI_Wtime();
		#endif

		if (try_perform_stack_op())
		{
			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			return;
		}
		else // if (!try_perform_stack_op())
		{
			// tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}
}

#endif /* STACK_EB2_NA_H */
