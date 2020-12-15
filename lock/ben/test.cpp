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

	struct Abc
	{
		int i;
		int j;
	};

	BCL::GlobalPtr<Abc> ptr = BCL::alloc<Abc>(1);
	Abc test;
	test.j = BCL::rank();
	BCL::rput_sync(test, ptr);
	//ptr->i = 0;
	std::cout << BCL::rank() << "\t" << ptr->j << std::endl;

  	BCL::finalize();
  	return 0;
}
