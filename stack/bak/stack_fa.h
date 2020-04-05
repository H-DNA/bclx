#ifndef STACK_FA_H
#define STACK_FA_H

#include "common.h"

namespace dds
{

namespace fas
{

	struct op_state
	{
		uint64_t 	pending : 1;
		uint64_t	id	: 63;
	};

	template <typename T>
	struct push_req
	{
		T 		value;
		op_state	state;
	};

	struct pop_req
	{
		uint64_t 	idx;
		op_state	state;
	};

	template <typename T>
	struct elem
	{
		T		value;
		push_req<T>	*push;
		pop_req		*pop;
		//uint64_t	offset;
	};

	template <typename T>
	struct segment
	{
		uint64_t	id;
		uint64_t	counter;
		//uint64_t	timestamp;
		bool		retired;
		int64_t		prev;
		int64_t		next;
		//segmment<T>	*free_next;
		gptr<elem<T>>	elems;
	};

	template <typename T>
	class stack
	{
	public:
		stack();		//collective
		~stack();		//collective
		void push(const T &);	//non-collective
		bool pop(T *);		//non-collective
		void print();		//collective

	private:
		const uint8_t		SEG_PER_UNIT = 1;
		const uint8_t		MAX_FAILURES = 10;

		gptr<gptr<segment<T>>>	topAddr;
		gptr<uint64_t>		indAddr;
		gptr<handle<T>>		hanAddr;

		gptr<gptr<segment<T>>>	topRep;
		gptr<uint64_t>		indRep;
	};

	template <typename T>
	struct handle
	{
		gptr<segment<T>>	topAddr;
		segment<T>	*sp;
		segment<T>	*free_list;
		handle<T>	*next;

		struct {
			uint64_t	help_id;
			push_req<T>	req;
			handle<T>	*peer;
		} push;

		struct {
			pop_req		req;
			handle<T>	*peer;
		} pop;

		uint64_t	timestamp;
	};

} /* namespace fas */

} /* namespace dds */

template <typename T>
dds::fas::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	topAddr = topRep = BCL::alloc<gptr<segment<T>>>(SEG_PER_UNIT);
	indAddr = indRep = BCL::alloc<uint64_t>(1);
	hanAddr = BCL::alloc<handle<T>>(1);

	if (BCL::rank() == MASTER_UNIT)
		BCL::store((uint64_t) 1, indAddr);
	else //if (BCL::rank() != MASTER_UNIT)
	{
		topAddr.rank = MASTER_UNIT;
		indAddr.rank = MASTER_UNIT;
	}

        //synchronize
	BCL::barrier();
}

template <typename T>
dds::fas::stack<T>::~stack()
{
        BCL::dealloc<gptr<segment<T>>>(topRep);
        BCL::dealloc<uint64_t>(indRep);
	BCL::dealloc<handle<T>>(hanAddr);
}

template <typename T>
void dds::fas::stack<T>::push(const T &value)
{
	gptr<gptr<segment<T>>>	hanTopAddr;
	gptr<segment<T>>	topVal;
	gptr<T>			valElemAddr;
	uint8_t			p;
	uint64_t		one,
				i;

        //h->time_stamp = get_timestamp();        //TODO

	hanTopAddr = {hanAddr.rank, hanAddr.ptr};
	topVal = BCL::rget_sync(topAdd);
	BCL::rput_sync(topVal, hanTopAddr);

        for (p = MAX_FAILURES; p > 0; --p)
        {
                //fast path
                one = 1;
                BCL::fetch_and_op_sync(indAddr, &one, BCL::add<uint64_t>{}, &i);
                c = find_cell(topVal, i);

		valElemAddr = {c.rank, c.ptr};
		BCL::compare_and_swap_sync(&valElemAddr, SPECIAL, x)
                if (BCL::compare_and_swap_sync(&c->elem, SPECIAL, x))
                        goto Line 13;
        }
        wf_push(h, x, p);       //slow path
        h->time_stamp = SPECIAL;
        h->top = null;
}

template <typename T>
bool dds::fas::stack<T>::pop(T *value)
{
	//TODO
}

template <typename T>
void dds::fas::stack<T>::print()
{
	//TODO
}

void find_cell(gptr<segment<T>> sgAddr, uint32_t cell_id)
{
	segment<T>	sgVal,
			topVal;

	sgVal = BCL::rget_sync(sgAddr);
	for (uint32_t i = sgVal.id; i < cell_id/N; ++i)
	{
		topVal = BCL::rget_sync(top);
		if (sg == topVal)
		{
			next = sg->next;	//next is not null because a dummy segment
						//is always next to the top segment
			nnext = next->next;
			if (nnext == nullptr)
			{
				tmp = new_segment(i + 2);
				tmp->prev = next;
				if (!CAS(&next->next, null, tmp)
					free(tmp);
			}
			CAS(&s->top, sg, next);
		}
		else
			next = sg->next;
		sg = next;
	}
	*sp = sg;
	return &sg->cells[cell_id % N];
}

void dds::fas::stack<T>::wf_push(handle *h, T x, uint32_t push_id)
{
	//publish my push request
	r = &h->push.req;
	r->elem = x;
	r->state = (1, push_id);
	sp = h->top;
	do {
		i = FAA(&s->T, 1);
		c = find_cell(&sp, i);
		if ((CAS(&c->push, SPECIAL, r) || c->push == r) &&
				(CAS(&r->state, (1, push_id), (0, i)) || r->state == (0, i))
			break;
		if (c->push == r)
			if (CAS(&c->pop, SPECIAL, SPECIAL))
			{
				counter = FAA(sp->counter, 1);
				if (counter == N - 1)
					remove(h, sp);
			}
	} while (r->state.pending == 0);
	i = r->state.id;
	find_cell(&h->top, i);
	c->elem = x;
}

#endif /* STACK_FA_H */
