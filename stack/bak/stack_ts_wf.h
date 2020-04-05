#ifndef STACK_TS_WF_H
#define STACK_TS_WF_H

#include "common.h"
#include "utility.h"

namespace dds
{

namespace tss_wf
{

	struct timestamp
	{
		uint64_t	start;
		uint64_t	end;

		bool operator<(const timestamp &) const;
	};

	class time
	{
	public:
                static const uint64_t    	ULLI_MIN 	= 0;			//lower bound
		static const uint64_t		ULLI_MAX 	= 18446744073709551615U;//upper bound
		static constexpr timestamp	TS_MIN 		= {ULLI_MIN, ULLI_MIN};	//min TS
		static constexpr timestamp 	TS_MAX		= {ULLI_MAX, ULLI_MAX};	//max TS

		time();
		~time();
		timestamp getNewTS();

	private:
                static const uint32_t		DELAY    	= 1;			//microseconds

		gptr<uint64_t> 			clock; 			//logical clock of the unit
	};

	template <typename T>
	struct elem
	{
                gptr<elem<T>>		next;
		gptr<op_desc<T>>	taken;
		timestamp		ts;
		T			value;
	};

	template <typename T>
	class memory
	{
	public:
		memory();
		~memory();
		gptr<elem<T>> malloc();
		void free(const gptr<elem<T>> &);

	private:
                gptr<elem<T>>			pool;           //allocates global memory
                gptr<elem<T>>			poolReplica;    //deallocates global memory
                uint64_t			capacity;       //contains global memory capacity (bytes)
		sds::stack<gptr<elem<T>>>	rlist;		//contains a local stack of reclaimed elems
	};

        template <typename T>
        struct help_record
        {
                gptr<elem<T>>           elemAddr;
        };

        template <typename T>
        struct op_desc
        {
                gptr<help_record<T>>    record;
                timestamp               startTime;
        };

        template <typename T>
        struct state
        {
                gptr<op_desc<T>>        op;
                uint32_t                peerID;

                state();
                ~state();
        };

	template <typename T>
	class stack
	{
        public:
                stack();                //collective
                ~stack();               //collective
                void push(const T &);	//non-collective
                bool pop(T *);          //non-collective
                void print();           //collective

        private:
        	static const gptr<elem<T>>	NULL_PTR 	= nullptr;
		static const uint32_t		MAX_FAILURES	= 10;

                gptr<gptr<elem<T>>>		top;
		state				sta;
		memory<T>			mem;
		time				tim;

		gptr<elem<T>> get_youngest(const gptr<elem<T>> &);
		bool remove(const gptr<elem<T>> &, const gptr<elem<T>> &, T *);
		bool try_rem(const timestamp &, bool &, T *);
		help_record *find_candidate(const timestamp &, op_desc *);
		void help_op(state *);
		void relax_remove(elem *, elem *);
		elem wf_rem(const timestamp &, state *);
	};

} /* namespace tss_wf */

} /* namespace dds */

bool dds::tss_wf::timestamp::operator<(const timestamp &ts) const
{
        return (this->end < ts.start);
}

dds::tss_wf::time::time()
{
	//synchronize
	BCL::barrier();

        clock = BCL::alloc<uint64_t>(1);
        BCL::rput((uint64_t) 0, clock);

	//synchronize
	BCL::barrier();
}

dds::tss_wf::time::~time()
{
        BCL::dealloc<uint64_t>(clock);
}

dds::tss_wf::timestamp dds::tss_wf::time::getNewTS()
{
	gptr<uint64_t> 	clockAddr;
	uint64_t 	clockVal;
	timestamp	ts;
	uint32_t	i;

	clockAddr.ptr = clock.ptr;

	ts.start = ULLI_MIN;
	for (i = 0; i < BCL::nprocs(); ++i)
	{
		clockAddr.rank = i;
		clockVal = BCL::aget_sync(clockAddr);
		if (clockVal > ts.start)
			ts.start = clockVal;
	}
	++(ts.start);
        BCL::aput_sync(ts.start, clock);

	//delay
	usleep(DELAY);

        ts.end = ULLI_MIN;
        for (i = 0; i < BCL::nprocs(); ++i)
        {
                clockAddr.rank = i;
                clockVal = BCL::aget_sync(clockAddr);
                if (clockVal > ts.end)
                        ts.end = clockVal;
        }
        ++(ts.end);
        BCL::aput_sync(ts.end, clock);

	return ts;
}

template <typename T>
dds::tss_wf::memory<T>::memory()
{
	//synchronize
	BCL::barrier();

        poolReplica = pool = BCL::alloc<elem<T>>(ELEM_PER_UNIT);
        capacity = pool.ptr + ELEM_PER_UNIT * sizeof(elem<T>);

	//synchronize
	BCL::barrier();
}

template <typename T>
dds::tss_wf::memory<T>::~memory()
{
        BCL::dealloc<elem<T>>(poolReplica);
}

template <typename T>
dds::gptr<dds::tss_wf::elem<T>> dds::tss_wf::memory<T>::malloc()
{
	gptr<elem<T>>	addr;

	if (rlist.pop(&addr) != EMPTY)
		return addr;
	if (pool.ptr < capacity)
		return pool++;
	return nullptr;
}

template <typename T>
void dds::tss_wf::memory<T>::free(const gptr<elem<T>> &addr)
{
	rlist.push(addr);
}

template <typename T>
void dds::tss_wf::state<T>::state()
{
	op = BCL::alloc<op_desc<T>>(1);
	peerID = (BCL::rank() + 1) % BCL::nprocs();
}

template <typename T>
void dds::tss_wf::state<T>::~state()
{
	BCL::dealloc<op_desc<T>>(op);
}

template <typename T>
dds::tss_wf::stack<T>::stack()
{
	//synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	BCL::store(NULL_PTR, top);

	//synchronize
	BCL::barrier();
}

template <typename T>
dds::tss_wf::stack<T>::~stack()
{
        BCL::dealloc<gptr<elem<T>>>(top);
}

template <typename T>
void dds::tss_wf::stack<T>::push(const T &value)
{
        timestamp		ts;
	gptr<timestamp>		addrTemp;
	gptr<elem<T>>		oldTopAddr,
				newTopAddr;
	elem<T>			newTopVal;

	//Line number 2
        oldTopAddr = BCL::aget_sync(top);

	//Line number 3
        newTopAddr = mem.malloc();
        if (newTopAddr == nullptr)
        {
                printf("[%lu]ERROR: The stack is full now. The push is ineffective.\n", BCL::rank());
                return;
        }
        newTopVal = {oldTopAddr, nullptr, tim.TS_MAX, value};
	BCL::store(newTopVal, newTopAddr);
       	BCL::aput_sync(newTopAddr, top);

	//Line number 4
	ts = tim.getNewTS();

	//Line number 15
        addrTemp = {newTopAddr.rank, newTopAddr.ptr + sizeof(gptr<elem<T>>) + sizeof(uint64_t)};
	BCL::aput_sync(ts, addrTemp);
}

template <typename T>
bool dds::tss_wf::stack<T>::pop(T *value)
{
        timestamp       startTime = tim.getNewTS();
	uint32_t 	tryTimes = 0;
	bool 		success,
			result = NON_EMPTY;

	do {
		success = try_rem(startTime, result, value);
	} while (!success && ++tryTimes<MAX_FAILURES>);

	/**//*
	//ask for help to complete, new in WF_TSS
	if (!success)
		element = wfRem(startTime, myState);

	/**//*
	if (element != EMPTY)
	{
		//new in WF_TSS
		state *peerState = &state[myState.peerID];
		helpOp(peerState); //help my peer if it needs help
		//set the next thread as my peer
		myState.peerID = (myState.peerID + 1) % BCL::nprocs();
	}
	/**/
}

template <typename T>
void dds::tss_wf::stack<T>::print()
{
	//synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<gptr<elem<T>>> 	topTemp;
		gptr<elem<T>>		topAddr;
		elem<T>			topVal;

		topTemp.ptr = top.ptr;
		for (uint32_t i = 0; i < BCL::nprocs(); ++i)
		{
			topTemp.rank = i;
			for (topAddr = BCL::rget_sync(topTemp); topAddr != nullptr; topAddr = topVal.next)
			{
				topVal = BCL::rget_sync(topAddr);
				//if (!topVal.taken)
				//{
					printf("[%d]value = %d, ts = {%lu, %lu}\n",
							i, topVal.value, topVal.ts.start, topVal.ts.end);
					topVal.next.print();
				//}
			}
		}
	}

	//synchronize
	BCL::barrier();
}

template <typename T>
dds::gptr<dds::tss_wf::elem<T>> dds::tss_wf::stack<T>::get_youngest(const gptr<elem<T>> &topAddr, const gptr<op_desc<T>> &op)
{
	elem<T>		temp;
	uint32_t 	checkTimes	= 0;

	while (true)
	{
		if (checkTimes == pow((BCL::nprocs() + 1), 2))
		{
			//this pop does not ask for help
			if (BCL::aget_sync(op) == nullptr)
				return <nullptr, nullptr>;	//give a contention indication

			help_record *record = op.record;
			if (record != nullptr && (record.node == nullptr || record.node.taken == op))
			//indicate that the helped pop is not pending
				return <nullptr, nullptr>;
		}

		if (result.taken == nullptr || result.taken == op)
			//return the first un-taken node
			return <result, oldTop>;
		else if (result.next == result)
			//indicate that the pool is empty
			return <null, oldTop>;
		result = result.next;
		++checkTimes;
	}

	/*for (; topAddr != nullptr; topAddr = temp.next)
	{
		temp = BCL::aget_sync(topAddr);
		if (!temp.taken)
			return topAddr;
	}
	return nullptr;*/
}

template <typename T>
bool dds::tss_wf::stack<T>::remove(gptr<elem<T>> &youngest, T *value)
{
	op_desc<T> 		oldVal = nullptr,
				newVal = ,
				res_b;
	elem<T>			temp;
	gptr<op_desc<T>>	youngestTemp;

	youngestTemp = {youngest.rank, youngest.ptr + sizeof(gptr<elem<T>>)};
	BCL::compare_and_swap_sync(youngestTemp, &oldValue, &newValue, &result);
	if (result == oldValue)
	{
		relax_remove(oldTop, node);	//TODO

		temp = BCL::aget_sync(youngest);
		*value = temp.value;
		return true;
	}

	value = nullptr;	//optional
	return false;
}

template <typename T>
bool dds::tss_wf::stack<T>::try_rem(const timestamp &startTime, bool *result, T *value)
{
	uint32_t		i;
	timestamp		tsMax;
	gptr<gptr<elem<T>>>	topTemp;
	gptr<elem<T>>		topAddr,
				tempAddr,
				youngestAddr,
				empty[BCL::nprocs()];
	elem<T>			tempVal;

	youngestAddr = nullptr;
	tsMax = tim.TS_MIN;
	topTemp.ptr = top.ptr;

	for (i = 0; i < BCL::nprocs(); ++i)
	{
		topTemp.rank = i;
		topAddr = BCL::aget_sync(topTemp);
		tempAddr = get_youngest(topAddr, nullptr);

		//give a contention indication because the first
		//num_threads + 1 nodes are taken, new in WF_TSS
		if (topAddr == nullptr)
		{
        		value = NULL;   //optional
        		return false;
		}

		//emptiness check
		if (tempAddr == nullptr)
		{
			empty[i] = topAddr;
			continue;
		}

		tempVal = BCL::aget_sync(tempAddr);

		//elimination
		if (startTime < tempVal.ts)
			return remove(topAddr, tempAddr, value);

		if (tsMax < tempVal.ts)
		{
			youngestAddr = tempAddr;
			tsMax = tempVal.ts;
		}
	}

	//emptiness check
	if (youngestAddr == nullptr)
	{
		value = nullptr;
		for (i = 0; i < BCL::nprocs(); ++i)
		{
			topTemp.rank = i;
			topAddr = BCL::aget_sync(topTemp);
			if (topAddr != empty[i])
				return false;
		}

		result = EMPTY;
		return true;
	}

	return remove(topAddr, youngestAddr, value);
}

/*
template <typename T>
help_record *dds::wf_tss::stack<T>::find_candidate(timestamp startTime, op_desc *op)
{
	HelpRecord *record = createHelpRecord();
	... //lines 62-66
	for each (SPPool current in spPools)
	{
		... //lines 69-70
		<node, poolTop> = get_youngest(op);
		if (poolTop == nullptr)
			goto line 191;
		... //lines 79-83
		//elimination
		if (startTime < nodeTimestamp && node.taken == nullptr)
		{
			record.node = node;
			record.oldTop = poolTop;
			return record;
		}
		... //lines 87-92
	}

	//emptiness check
	if (youngest == nullptr)
	{
		for each (SPPool current in spPools)
			if (current.top != empty[current.ID]
				goto line 191;

		record.node = nullptr;
		return record;
	}

	if (youngest.taken == nullptr)
	{
		record.node = youngest;
		record.oldTop = top;
		return record;
	}

	free(record);
	return nullptr;
}

template <typename T>
void dds::tss_wf::stack<T>::help_op(gptr<state<T>> helpee)
{
	op_desc *op = helpee.op;
	if (op == nullptr) //the helpee does not need help
		return;
	help_record *pRecord = op.record, *lRecord = nullptr;
	Timestamp startTime = op.startTime;
	while (true)
	{
		if (pRecord != nullptr)
		{
			node *node = pRecord.node;
			if (node == nullptr || node.taken == op || CAS(&(node.taken), nullptr, op) || node.taken == op)
				return; //the helpee does not need help yet
		}

		if (lRecord == nullptr || (lRecord.node != nullptr && lRecord.node.taken != nullptr && lRecode.node.taken == op)
			lRecord = find_candidate(startTime, op);

		if (lRecord != null)
			if (CAS(&(op.record) , pRecord, lRecord))
				lRecord = nullptr;
		pRecord = op.record;
	}
}

template <typename T>
void dds::tss_wf::stack<T>::relax_remove(elem *oldTop, elem *elem)
{
	elem *next = elem.next;
	//find the next un-taken node
	while (next.next != nullptr && next.taken != nullptr)
		next = next.next;
	elem topNext = oldTop.next;
	//ensure oldTop.next not to point to a node inserted after next
	while (next.timestamp < topNext.timestamp && !CAS(&(oldTop.next), topNext, next))
		topNext = oldTop.next;
	elem elemNext = elem.next;
	//ensure elem.next not to point to a node inserted after next
	while (next.timestamp < elemNext.timestamp && !CAS(&(elem.next), elemNext, next))
		elemNext = elem.next;
}

template <typename T>
T dds::tss_wf::stack<T>::wf_rem(const timestamp &startTime, gptr<state<T>> state)
{
	op_desc *op = create_op_desc();
	op.startTime = startTime;
	op.record = null;
	//atomic assignment: announce the operation
	state.op = op;
	//help my own operation to complete
	help_op(state);
	op = state.op;
	help_record record = op.record;
	elem<T> node = record.node;
	//atomic assignment: report that I am not pending
	state.op = null;
	if (node == null)
		return EMPTY;
	elem<T> oldTop = record.oldTop;
	relax_remove(oldTop, node); //remove the taken node
	return node.value;
}
/**/

#endif /* STACK_TS_WF_H */
