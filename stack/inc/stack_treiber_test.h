#ifndef STACK_TREIBER_TEST_H
#define STACK_TREIBER_TEST_H

#include <bclx/core/util/backoff.hpp>

namespace dds
{

namespace ts_test
{

	/* Macros */
	#ifdef 		MEM_HP
		using namespace hp;
	#elif defined	MEM_EBR2
		using namespace ebr2;	
	#else
		using namespace dang3;
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
		stack();			// collective
		stack(const uint64_t &num);	// collective
		~stack();			// collective
		bool push(const T &value);	// non-collective
		bool pop(T &value);		// non-collective
		void print();			// collective

	private:
        	const gptr<elem<T>> 	NULL_PTR = nullptr; 	// be a null constant

		memory<elem<T>>		mem;	// manage global memory
                gptr<gptr<elem<T>>>	top;	// point to global address of the top
		uint64_t		bk_i;

		bool push_fill(const T &value);
	};

} /* namespace ts_test */

} /* namespace dds */

template<typename T>
dds::ts_test::stack<T>::stack()
{
	// synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
                BCL::store(NULL_PTR, top);
		stack_name = "TS_test";
	}
	else
		top.rank = MASTER_UNIT;
	bk_i = exp2l(0);

	// synchronize
	BCL::barrier();
}

template<typename T>
dds::ts_test::stack<T>::stack(const uint64_t &num)
{
	// synchronize
	BCL::barrier();

	top = BCL::alloc<gptr<elem<T>>>(1);
	if (BCL::rank() == MASTER_UNIT)
	{
		BCL::store(NULL_PTR, top);
		stack_name = "TS_test";

		for (uint64_t i = 0; i < num; ++i)
			push_fill(i);
	}
	else
		top.rank = MASTER_UNIT;

        // synchronize
        BCL::barrier();
}

template<typename T>
dds::ts_test::stack<T>::~stack()
{
	if (BCL::rank() != MASTER_UNIT)
		top.rank = BCL::rank();
	BCL::dealloc<gptr<elem<T>>>(top);
}

template<typename T>
bool dds::ts_test::stack<T>::push(const T &value)
{
        gptr<elem<T>> 		oldTopAddr,
				newTopAddr;
	backoff::backoff        bk(bk_i, bk_max);

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
			printf("The stack is FULL\n");
			++fail_cs;
		#endif

		return false;
	}

	while (true)
	{
		// tracing
		#ifdef	TRACING
			start = MPI_Wtime();
		#endif

		// get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		// update new element (global memory)
                BCL::rput_sync({oldTopAddr, value}, newTopAddr);

		// update top (global memory)
		if (BCL::cas_sync(top, oldTopAddr, newTopAddr) == oldTopAddr)
		{
			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			if (bk_i >= 2)
				bk_i /= 2;

			return true;
		}
		else // if (BCL::cas_sync(top, oldTopAddr, newTopAddr) != oldTopAddr)
		{
			bk_i = bk.delay_dbl();

			// tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}
}

template<typename T>
bool dds::ts_test::stack<T>::pop(T &value)
{
	// begin a nonblocking operation
	mem.op_begin();

	elem<T> 		oldTopVal;
	gptr<elem<T>> 		oldTopAddr;
	backoff::backoff        bk(bk_i, bk_max);

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

		// get top (from global memory to local memory)
		oldTopAddr = BCL::aget_sync(top);

		if (oldTopAddr == nullptr)
		{
			// tracing
			#ifdef	TRACING
				printf("The stack is EMPTY\n");
				++succ_cs;
			#endif

			return false;
		}

		// try to reserve top
		if (!mem.try_reserve(oldTopAddr, top))
		{
			// unreserve top
			mem.unreserve(oldTopAddr);

			continue;
		}

		// get node (from global memory to local memory)
		oldTopVal = BCL::rget_sync(oldTopAddr);

		// update top
		if (BCL::cas_sync(top, oldTopAddr, oldTopVal.next) == oldTopAddr)
		{
			// unreserve top
			mem.unreserve(oldTopAddr);

			// tracing
			#ifdef	TRACING
				++succ_cs;
			#endif

			if (bk_i >= 2)
				bk_i /= 2;

			break;
		}
		else // if (BCL::cas_sync(top, oldTopAddr, oldTopVal.next) != oldTopAddr)
		{
			// unreserve top
			mem.unreserve(oldTopAddr);

			bk_i = bk.delay_dbl();

			//tracing
			#ifdef	TRACING
				fail_time += (MPI_Wtime() - start);
				++fail_cs;
			#endif
		}
	}

	// return the value of the popped elem
	value = oldTopVal.value;

	// deallocate global memory of the popped elem
	mem.free(oldTopAddr);

	// end a nonblocking operation
	mem.op_end();

	return true;
}

template<typename T>
void dds::ts_test::stack<T>::print()
{
	// synchronize
	BCL::barrier();

	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<elem<T>>	topAddr;
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
bool dds::ts_test::stack<T>::push_fill(const T &value)
{
	if (BCL::rank() == MASTER_UNIT)
	{
		gptr<elem<T>>		oldTopAddr,
					newTopAddr;

		// allocate global memory to the new elem
		newTopAddr = mem.malloc();
		if (newTopAddr == nullptr)
			return false;

		// get top (from global memory to local memory)
		oldTopAddr = BCL::load(top);

		// update new element (global memory)
		BCL::store({oldTopAddr, value}, newTopAddr);

		// update top (global memory)
		BCL::store(newTopAddr, top);
		
		return true;
	}
}

#endif /* STACK_TREIBER_TEST_H */

