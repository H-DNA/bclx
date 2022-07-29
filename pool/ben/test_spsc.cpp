#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...
#include "../inc/pool.h"	// dds::pool_ubd_spsc...

int main()
{
	using namespace bclx;

	BCL::init();

	gptr<dds::block<int>> handle = BCL::alloc<dds::block<int>>(1024);
	BCL::barrier();
	
	dds::pool_ubd_spsc<int> my_pool(0);

	if (BCL::rank() == 0)
	{
		for (int i = 0; i < 2; ++i)
		{
			BCL::barrier();	// synchronize

			gptr<dds::header> head, tail;
			my_pool.get(head, tail);

			// debugging
			for (gptr<dds::header> tmp = head; tmp != tail; tmp = rget_sync(tmp).next)
				printf("[%lu]CP20: <%u, %u>\n", BCL::rank(), tmp.rank, tmp.ptr);
			printf("[%lu]CP20: <%u, %u>\n", BCL::rank(), tail.rank, tail.ptr);
		}
	}
	else // if (BCL::rank() == 1)
	{
		std::vector<gptr<int>>	dang;
		for (int j = 0; j < 2; ++j)
		{
			if (j == 1)
				handle += 500;
			for (int i = 0; i < 3; ++i)
			{
				dang.push_back({0, handle.ptr + sizeof(dds::header)});
				if (j == 0)
					handle += 4;
				else // if (j == 1)
					handle -= 100;
			}

			// debugging
			for (const auto &x : dang)
				printf("[%lu]CP21: <%u, %u>\n", BCL::rank(), x.rank, x.ptr);

			my_pool.put(dang);

			dang.clear();

			BCL::barrier();	// synchronize
		}
	}

	BCL::barrier();
	BCL::dealloc<dds::block<int>>(handle);

	BCL::finalize();

	return 0;
}
