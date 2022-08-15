#ifndef STACK_TREIBER_H
#define STACK_TREIBER_H

namespace dds
{

namespace ts
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
#elif defined	MEM_BL
	using namespace bl;
#elif defined	MEM_BL2
	using namespace bl2;
#elif defined	MEM_BL3
	using namespace bl3;
#else	// No Memory Reclamation
	using namespace nmr;
#endif

/* Data types */
template<typename T>
struct elem
{
        gptr<elem<T>>   next;
        T               value;
};

template<typename T>
class stack
{
public:
	memory<elem<T>>		mem;	// manage global memory

	stack();			// collective
	stack(const uint64_t &num);	// collective
	~stack();			// collective
	bool push(const T &value);	// non-collective
	bool pop(T &value);		// non-collective
	void print();			// collective

private:
	const gptr<elem<T>> 	NULL_PTR = nullptr; 	// be a null constant

        gptr<gptr<elem<T>>>	top;	// point to global address of the top

	bool push_fill(const T &value);
};

} /* namespace ts */

} /* namespace dds */

template<typename T>
dds::ts::stack<T>::stack()
{
	// synchronize
	bclx::barrier_sync();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		bclx::store(NULL_PTR, top);
		stack_name = "TS";
	}
	else
		top.rank = MASTER_UNIT;

	// synchronize
	bclx::barrier_sync();
}

template<typename T>
dds::ts::stack<T>::stack(const uint64_t &num)
{
	// synchronize
	bclx::barrier_sync();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		bclx::store(NULL_PTR, top);
		stack_name = "TS";

		for (uint64_t i = 0; i < num; ++i)
			push_fill(i);
	}
	else
		top.rank = MASTER_UNIT;

        // synchronize
	bclx::barrier_sync();
}

template<typename T>
dds::ts::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);
}

template<typename T>
bool dds::ts::stack<T>::push(const T &value)
{
        gptr<elem<T>> 	oldTopAddr,
			newTopAddr;
	backoff		bk(bk_init, bk_max);

	// tracing
	#ifdef	TRACING
		double		start;
	#endif

	// allocate global memory to the new elem
	newTopAddr = mem.malloc();
	if (newTopAddr == nullptr)
	{
		// tracing
		#ifdef	TRACING
			++fail_cs;
		#endif

		printf("[%lu]ERROR: stack.push\n", BCL::rank());
		return false;
	}

	while (true)
	{
		// tracing
		#ifdef	TRACING
			start = MPI_Wtime();
		#endif

		// get top (from global memory to local memory)
		oldTopAddr = bclx::aget_sync(top);

		// update new element (global memory)
		#ifdef	MEM_DANG3
			bclx::store({oldTopAddr, value}, newTopAddr);
		#else
			bclx::rput_sync({oldTopAddr, value}, newTopAddr);
		#endif

		// update top (global memory)
		if (bclx::cas_sync(top, oldTopAddr, newTopAddr) == oldTopAddr)
		{
			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			return true;
		}
		else // if (bclx::cas_sync(top, oldTopAddr, newTopAddr) != oldTopAddr)
		{
			bk.delay_dbl();

			// tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}
}

template<typename T>
bool dds::ts::stack<T>::pop(T &value)
{
	// begin a nonblocking operation
	mem.op_begin();

	elem<T> 	oldTopVal;
	gptr<elem<T>> 	oldTopAddr,
			result;
	backoff		bk(bk_init, bk_max);

	// tracing
	#ifdef  TRACING
		double		start;
	#endif

	while (true)
	{
		// tracing
		#ifdef	TRACING
			start = MPI_Wtime();
		#endif

		// reserve and get top
		oldTopAddr = mem.reserve(top);

		// check if the stack is empty
		if (oldTopAddr == nullptr)
		{
			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			// end a nonblocking operation
			mem.op_end();

			printf("[%lu]ERROR: stack.pop\n", BCL::rank());
			return false;
		}

		// get node (from global memory to local memory)
		oldTopVal = bclx::rget_sync(oldTopAddr);

		// try to update top
		result = bclx::cas_sync(top, oldTopAddr, oldTopVal.next);
		
		// unreserve top
		mem.unreserve(oldTopAddr);

		// check if the update is successful
		if (result == oldTopAddr)
		{
			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			break;
		}
		else // if (result != oldTopAddr)
		{
			bk.delay_dbl();

			// tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}

	// return the value of the popped elem
	value = oldTopVal.value;

	// deallocate global memory of the popped elem
	mem.retire(oldTopAddr);

	// end a nonblocking operation
	mem.op_end();

	return true;
}

template<typename T>
void dds::ts::stack<T>::print()
{
	// synchronize
	bclx::barrier_sync();

	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<elem<T>>	topAddr;
		elem<T>		topVal;

		for (topAddr = bclx::load(top); topAddr != nullptr; topAddr = topVal.next)
		{
			topVal = bclx::rget_sync(topAddr);
                	printf("value = %d\n", topVal.value);
                	topVal.next.print();
		}
	}

	// synchronize
	bclx::barrier_sync();
}

template<typename T>
bool dds::ts::stack<T>::push_fill(const T &value)
{
	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<elem<T>>		oldTopAddr,
					newTopAddr;

		// allocate global memory to the new elem
		newTopAddr = mem.malloc();
		if (newTopAddr == nullptr)
		{
			printf("[%lu]ERROR: stack.push_fill\n", BCL::rank());
			return false;
		}

		// get top (from global memory to local memory)
		oldTopAddr = bclx::load(top);

		// update new element (global memory)
		#ifdef	MEM_DANG3
			bclx::store({oldTopAddr, value}, newTopAddr);
		#else
			bclx::rput_sync({oldTopAddr, value}, newTopAddr);
		#endif

		// update top (global memory)
		bclx::store(newTopAddr, top);
		
		return true;
	}
	else // if (BCL::rank() != MASTER_UNIT)
		return true;
}

#endif /* STACK_TREIBER_H */

