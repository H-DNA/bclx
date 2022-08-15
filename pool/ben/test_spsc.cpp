#include <cstdint>		// uint64_t...
#include <bclx/bclx.hpp>	// bclx...
#include "../inc/pool.h"	// dds::pool_ubd_spsc...

int main()
{
	using namespace bclx;

	BCL::init();

	gptr<dds::block<int>> handle = BCL::alloc<dds::block<int>>(1024);
	bclx::barrier_sync();
	
	dds::pool_ubd_spsc<int> my_pool(0);

	if (BCL::rank() == 0)
	{
		dds::list_seq rlist;
		for (int i = 0; i < 2; ++i)
		{
			bclx::barrier_sync();	// synchronize

			dds::list_seq slist;
			my_pool.get(slist);

			printf("----------------\n");
			slist.print();

			rlist.append(slist);
			printf("****************\n");
			rlist.print();
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

			bclx::barrier_sync();	// synchronize
		}
	}

	bclx::barrier_sync();
	BCL::dealloc<dds::block<int>>(handle);

	BCL::finalize();

	return 0;
}
