#ifndef STACK_EB_NA_H
#define STACK_EB_NA_H

#include <random>
#include "../../lib/backoff.h"
#include "../../lib/ta.h"

namespace dds
{

namespace ebs_na
{

	/* Macros */
	#ifdef MEM_REC
		using namespace hp;
	#else
		using namespace dang3;
	#endif

	/* Data types */
	struct adapt_params
	{
		uint32_t 	count;
		float 		factor;
	};

        template <typename T>
        struct elem
        {
                gptr<elem<T>>   next;
                T               value;
        };

	template <typename T>
	struct unit_info
	{
		gptr<elem<T>>	itsElem;
		uint32_t	rank;
		char		op;
	};

	template <typename T>
	class stack
	{
	public:
		stack();			//collective
		stack(const uint64_t &num);	//collective
		~stack();			//collective
		bool push(const T &value);	//non-collective
		bool pop(T &value);		//non-collective
		void print();			//collective

	private:
       		const gptr<elem<T>> 		NULL_PTR_E = 	nullptr;
		const gptr<unit_info<T>>	NULL_PTR_U = 	nullptr;
        	const uint32_t       		NULL_UNIT =	-1;
        	const uint32_t       		COUNT_INIT =	5;
                const uint32_t			COUNT_MAX =     2 * COUNT_INIT;
		const float			FACTOR_INIT =	0.5;
        	const float     		FACTOR_MIN =	0.0;
        	const float     		FACTOR_MAX =	1.0;
        	#define         		COLL_SIZE	BCL::nprocs()
                const bool        		PUSH = 		true;
                const bool        		POP =		false;
                const bool        		SHRINK = 	true;
                const bool        		ENLARGE =	false;

		memory<elem<T>>			mem;		//contains essential stuffs to manage globmem of elems
                gptr<gptr<elem<T>>>		top;		//contains global memory address of the dummy node
		gptr<gptr<unit_info<T>>>	location;	//contains global mem add of a gptr of a @unit_info
		gptr<unit_info<T>>		p;
		gptr<uint32_t>			collision;	//contains the rank of the unit trying to collide
                adapt_params			adapt;          //contains the adaptative elimination backoff

		ta::na				na;		//contains node information

		bool push_fill(const T &value);
		uint32_t get_position();
		void adapt_width(const bool &);
		bool try_collision(const gptr<unit_info<T>> &, const uint32_t &);
		void finish_collision();
                bool try_perform_stack_op();
                void less_op();
                void stack_op();
	};

} /* namespace ebs_na */

} /* namespace dds */

template<typename T>
dds::ebs_na::stack<T>::stack()
{
        //synchronize
	BCL::barrier();

        location = BCL::alloc<gptr<unit_info<T>>>(1);
        BCL::store(NULL_PTR_U, location);

        p = BCL::alloc<unit_info<T>>(1);

        collision = BCL::alloc<uint32_t>(ceil(COLL_SIZE / BCL::nprocs()));
        BCL::store(NULL_UNIT, collision);

        adapt = {COUNT_INIT, FACTOR_INIT};

        top = BCL::alloc<gptr<elem<T>>>(1);
        if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR_E, top);
		stack_name = "EBS_NA";
	}
	else //if (BCL::rank() != MASTER_UNIT)
		top.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::ebs_na::stack<T>::stack(const uint64_t &num)
{
	//synchronize
	BCL::barrier();

	location = BCL::alloc<gptr<unit_info<T>>>(1);
	BCL::store(NULL_PTR_U, location);

	p = BCL::alloc<unit_info<T>>(1);

	collision = BCL::alloc<uint32_t>(ceil(COLL_SIZE / BCL::nprocs()));
	BCL::store(NULL_UNIT, collision);

	adapt = {COUNT_INIT, FACTOR_INIT};

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		BCL::store(NULL_PTR_E, top);
		stack_name = "EBS_NA";

		for (uint64_t i = 0; i < num; ++i)
			push_fill(i);
	}
	else //if (BCL::rank() != MASTER_UNIT)
		top.rank = MASTER_UNIT;

	//synchronize
	BCL::barrier();
}

template<typename T>
dds::ebs_na::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);

	collision.rank = BCL::rank();
        BCL::dealloc<uint32_t>(collision);

        BCL::dealloc<unit_info<T>>(p);

	location.rank = BCL::rank();
	BCL::dealloc<gptr<unit_info<T>>>(location);
}

template<typename T>
bool dds::ebs_na::stack<T>::push(const T &value)
{
	unit_info<T> 	temp;
	gptr<T>		tempAddr;

	temp.rank = BCL::rank();
	temp.op = PUSH;
	//allocate global memory to the new elem
	temp.itsElem = mem.malloc();
	if (temp.itsElem == nullptr)
	{
		//tracing
		#ifdef	TRACING
                	printf("The stack is FULL\n");
			++fail_cs;
		#endif

		return false;
	}

	tempAddr = {temp.itsElem.rank, temp.itsElem.ptr + sizeof(gptr<elem<T>>)};
	#ifdef MEM_REC
		BCL::rput_sync(value, tempAddr);
	#else
		BCL::store(value, tempAddr);
	#endif
	BCL::store(temp, p);

	stack_op();

	return true;
}

template<typename T>
bool dds::ebs_na::stack<T>::pop(T &value)
{
	unit_info<T> temp;

	temp.rank = BCL::rank();
	temp.op = POP;
	BCL::store(temp, p);

	stack_op();

	gptr<gptr<elem<T>>> tempAddr = {p.rank, p.ptr};
	gptr<elem<T>> tempAddr2 = BCL::load(tempAddr);

	if (tempAddr2 == NULL_PTR_E)
	{
		//tracing
		#ifdef	TRACING
			//printf("The stack is EMPTY\n");
		#endif

		return false;
	}
	else
	{
		gptr<T>	tempAddr3 = {tempAddr2.rank, tempAddr2.ptr + sizeof(gptr<elem<T>>)};
		value = BCL::rget_sync(tempAddr3);

                //deallocate global memory of the popped elem
                mem.free(tempAddr2);

		return true;
	}
}

template<typename T>
void dds::ebs_na::stack<T>::print()
{
	//synchronize
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

	//synchronize
	BCL::barrier();
}

template<typename T>
bool dds::ebs_na::stack<T>::push_fill(const T &value)
{
        if (BCL::rank() == MASTER_UNIT)
        {
                unit_info<T>    temp;
                gptr<T>         tempAddr;
                gptr<elem<T>>   oldTopAddr;

                temp.rank = BCL::rank();
                temp.op = PUSH;
                //allocate global memory to the new elem
                temp.itsElem = mem.malloc();
                if (temp.itsElem == nullptr)
		{
			printf("The stack is FULL\n");
                        return false;
		}

                //get top (from global memory to local memory)
                oldTopAddr = BCL::load(top);

                //update new element (global memory)
                BCL::store({oldTopAddr, value}, temp.itsElem);
                BCL::store(temp, p);

                //update top (global memory)
                BCL::store(temp.itsElem, top);

                return true;
        }
}

template<typename T>
bool dds::ebs_na::stack<T>::try_perform_stack_op()
{
	unit_info<T>		pVal;
	gptr<elem<T>>		oldTopAddr;
	gptr<gptr<elem<T>>>	tempAddr;

	pVal = BCL::load(p);
	if (pVal.op == PUSH)
	{
		gptr<gptr<elem<T>>>	newTopAddr;

		//get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		//update new element (global memory)
        	tempAddr = {pVal.itsElem.rank, pVal.itsElem.ptr};
		#ifdef MEM_REC
			BCL::rput_sync(oldTopAddr, tempAddr);
		#else
			BCL::store(oldTopAddr, tempAddr);
		#endif

		//update top (global memory)
		if (BCL::cas_sync(top, oldTopAddr, pVal.itsElem) == oldTopAddr)
			return true;
		return false;
	}
	if (pVal.op == POP)
	{
		elem<T>		newTopVal;
		gptr<elem<T>>	oldTopAddr2;
		bool		res;

                tempAddr = {p.rank, p.ptr};

		//do {
			//get top (from global memory to local memory)
			oldTopAddr = BCL::aget_sync(top);

			if (oldTopAddr == nullptr)
			{
				BCL::store(NULL_PTR_E, tempAddr);

				return true;
			}

			//update hazard pointers
			/*BCL::aput_sync(oldTopAddr, mem.hp);
			oldTopAddr2 = BCL::aget_sync(top);
			/**/

		//} while (oldTopAddr != oldTopAddr2);

		//get node (from global memory to local memory)
		newTopVal = BCL::rget_sync(oldTopAddr);

		//update top
		if (BCL::cas_sync(top, oldTopAddr, newTopVal.next) == oldTopAddr)
		{
			BCL::store(oldTopAddr, tempAddr);

			res = true;
		}
		else
			res = false;

		//update hazard pointers
		//BCL::aput_sync(NULL_PTR_E, mem.hp);
		/**/

		return res;
	}
}

template<typename T>
uint32_t dds::ebs_na::stack<T>::get_position()
{
	uint32_t 	min, max;

	if (na.size % 2 == 0)
		min = round((na.size / 2 - 1) * (FACTOR_MAX - FACTOR_MIN - adapt.factor));
	else //if (na.size % 2 != 0)
		min = round(na.size / 2 * (FACTOR_MAX - FACTOR_MIN - adapt.factor));
	max = round(na.size / 2 * (FACTOR_MAX - FACTOR_MIN + adapt.factor));

	std::default_random_engine generator;
	std::uniform_int_distribution<uint32_t> distribution(min, max);
	return na.table[distribution(generator)];	//Generate number in the range min..max
}

template<typename T>
void dds::ebs_na::stack<T>::adapt_width(const bool &dir)
{
	if (dir == SHRINK)
	{
		if (adapt.count > 0)
			--adapt.count;
		else //if (adapt.count == 0)
		{
			adapt.count = COUNT_INIT;
			adapt.factor = std::max(adapt.factor / 2, FACTOR_MIN);
		}
	}
	else //if (dir == ENLARGE)
	{
		if (adapt.count < COUNT_MAX)
			++adapt.count;
		else //if (adapt.count == MAX_COUNT)
		{
			adapt.count = COUNT_INIT;
			adapt.factor = std::min(2 * adapt.factor, FACTOR_MAX);
		}
	}
}

template<typename T>
bool dds::ebs_na::stack<T>::try_collision(const gptr<unit_info<T>> &q, const uint32_t &him)
{
	location.rank = him;
	unit_info<T> pVal = BCL::load(p);
	if (pVal.op == PUSH)
	{
		if (BCL::cas_sync(location, q, p) == q)
			return true;
		else
		{
			adapt_width(ENLARGE);
			return false;
		}
	}
	else //if (pVal.op == POP)
	{
		if (BCL::cas_sync(location, q, NULL_PTR_U) == q)
		{
			gptr<elem<T>> tempVal = BCL::rget_sync((gptr<gptr<elem<T>>>) {q.rank, q.ptr});
			BCL::store(tempVal, (gptr<gptr<elem<T>>>) {p.rank, p.ptr});

			return true;
		}
		else
		{
			adapt_width(ENLARGE);
			return false;
		}
	}
}

template<typename T>
void dds::ebs_na::stack<T>::finish_collision()
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
void dds::ebs_na::stack<T>::less_op()
{
	uint32_t		myUID = BCL::rank(),
				pos,
				him;
	unit_info<T>		pVal,
				qVal;
	gptr<unit_info<T>>	q;
	backoff::backoff	bk(bk_init, bk_max);

	//tracing
	#ifdef	TRACING
		double		start;
	#endif

	while (true)
	{
		//tracing
		#ifdef	TRACING
			start = MPI_Wtime();
		#endif

		location.rank = myUID;
		BCL::aput_sync(p, location);
		pos = get_position();

		collision.rank = pos;
		do {
			him = BCL::aget_sync(collision);
		} while (BCL::cas_sync(collision, him, myUID) != him);

		if (him != NULL_UNIT)
		{
			location.rank = him;
			q = BCL::aget_sync(location);
			if (q != nullptr)
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
							//tracing
							#ifdef	TRACING
								++succ_ea;
							#endif

							return;
						}
						else
							goto label;
					}
					else
					{
						finish_collision();

						//tracing
						#ifdef	TRACING
							++succ_ea;
						#endif

						return;
					}
				}
			}
		}

		bk.delay_dbl();
		adapt_width(SHRINK);

		location.rank = myUID;
		if (BCL::cas_sync(location, p, NULL_PTR_U) != p)
		{
			finish_collision();

			//tracing
			#ifdef	TRACING
				++succ_ea;
			#endif

			return;
		}

	label:
		//tracing
		#ifdef	TRACING
			fail_time += (MPI_Wtime() - start);
			++fail_ea;
			start = MPI_Wtime();
		#endif

		if (try_perform_stack_op())
		{
			//tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			return;
		}
		else //if (!try_perform_stack_op())
		{
			//tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}
}

template<typename T>
void dds::ebs_na::stack<T>::stack_op()
{
	//tracing
	#ifdef	TRACING
		double start = MPI_Wtime();
	#endif

	if (try_perform_stack_op())
	{
		//tracing
		#ifdef	TRACING
			++succ_cs;
		#endif
	}
	else //if (!try_perform_stack_op())
	{
		//tracing
		#ifdef	TRACING
			fail_time += (MPI_Wtime() - start);
			++fail_cs;
		#endif

		less_op();
	}
}

#endif /* STACK_EB_NA_H */
