#include <vector>	// std::vector...
#include <cstdint>	// uint64_t...
#include <cstdio>	// printf...
#include <bclx/bcl.hpp>
#include "../inc/queue.h"

using namespace BCL;
using namespace dds;

int main()
{
	BCL::init();

	std::vector<std::vector<dds::queue_spsc<int>>>	queues;
        for (uint64_t i = 0; i < BCL::nprocs(); ++i)
        {
                queues.push_back(std::vector<dds::queue_spsc<int>>());
                for (uint64_t j = 0; j < BCL::nprocs(); ++j)
                        queues[i].push_back(dds::queue_spsc<int>(i, 100));
        }

	std::vector<int> vec;
	if (BCL::rank() % 2 == 0)
	{
		for (int i = 0; i < 3; ++i)
			vec.push_back(10 * BCL::rank() + i);
		queues[BCL::rank() + 1][BCL::rank()].enqueue(vec);
	}
	else // if (BCL::rank() % 2 != 0)
	{
		while (!queues[BCL::rank()][BCL::rank() - 1].dequeue(vec));
		for (auto& x : vec)
			printf("[%lu]%d\n", BCL::rank(), x);
		printf("[%lu]***********************\n", BCL::rank());
	}

	BCL::barrier();

	std::vector<int> vec2;
        if (BCL::rank() % 2 == 0)
        {
                for (int i = 0; i < 2; ++i)
                        vec2.push_back(10 * BCL::rank() + i);
                queues[BCL::rank() + 1][BCL::rank()].enqueue(vec2);
        }
        else // if (BCL::rank() % 2 != 0)
        {
                while (!queues[BCL::rank()][BCL::rank() - 1].dequeue(vec2));
                for (auto& x : vec2)
                        printf("[%lu]%d\n", BCL::rank(), x);
        }

	BCL::finalize();

	return 0;
}
