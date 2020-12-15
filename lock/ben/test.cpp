#include <iostream>
#include <thread>
#include <bcl/bcl.hpp>
#include "../inc/lock.h"

int main(int argc, char *argv[])
{
	BCL::init();

	dds::lock_tas l;

  	l.acquire();
  	int rank = BCL::rank();
  	int nprocs = BCL::nprocs();
  	std::cout << "(" << rank << "/" << nprocs << "): lock acquired" << std::endl;
	//std::this_thread::sleep_for(std::chrono::seconds(1));
  	std::cout << "(" << rank << "/" << nprocs << "): releasing lock" << std::endl;
  	l.release();

	BCL::GlobalPtr<int> dang;
	if (BCL::rank() == 1)
	{
		dang = BCL::alloc<int>(1);
		printf("CP2: %lu: %u, %u\n", BCL::rank(), dang.rank, dang.ptr);
	}

	BCL::GlobalPtr<int> you;
	you = BCL::alloc<int>(1);
	printf("CP3: %lu: %u, %u\n", BCL::rank(), you.rank, you.ptr);

  	BCL::finalize();
  	return 0;
}
