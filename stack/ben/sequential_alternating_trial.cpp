#include <iostream>
#include <thread>
#include <chrono>
#include <bcl/bcl.hpp>
#include "../inc/stack.h"
#include "../../lib/ta.h"

using namespace dds;
using namespace dds::ebs2_na;

template <typename T>
inline double work_time(stack<T> *my_stack, const uint64_t &num_ops, const uint32_t &left, const uint32_t &right);

int main()
{
        uint32_t	i,
			left,
			mid,
			right;
	uint64_t	num_ops;
	double		total_time_mid,
			total_time_right,
			*total_time_array;

        BCL::init();

	if (BCL::nprocs() % 2 != 0)
	{
		if (BCL::rank() == MASTER_UNIT)
			std::cout << "ERROR: The number of units must be even!\n";
		return -1;
	}

        left = log2l(bk_init);
        if (left == 0)
        {
                if (BCL::rank() == MASTER_UNIT)
                        std::cout << "ERROR: The exponent of bk_init must be greater than 0!\n";
                return -1;
        }
        right = log2l(bk_max);

	stack<uint32_t> *my_stack;
	num_ops = ELEMS_PER_UNIT / BCL::nprocs();

	//Step 1: determining the potential search space for the backoff range
	if (BCL::rank() == MASTER_UNIT)
		std::cout << std::endl <<
			"STEP 1: determining the potential search space of the backoff range\n\n";

	mid = (left + right) / 2;
	total_time_mid = 0;
	total_time_right = 0;
	while (true)
	{
		if (total_time_mid == 0)
		{
			my_stack = new stack<uint32_t>(ELEMS_PER_UNIT / 2);
			total_time_mid = work_time(my_stack, num_ops, left, mid);
			delete my_stack;
		}

		if (total_time_right == 0)
		{
			my_stack = new stack<uint32_t>(ELEMS_PER_UNIT / 2);
			total_time_right = work_time(my_stack, num_ops, left, right);
			delete my_stack;
		}

		if (BCL::rank() == MASTER_UNIT)
		{
			std::cout << "left = " << left <<
				", mid = " << mid << ", right = " << right << std::endl;
			std::cout << "total_time_mid = " << total_time_mid <<
				", total_time_right = " << total_time_right << "\n\n";
		}

		if (total_time_mid >= total_time_right)
		{
			if (right <= 25)
			{
				mid = right;
				right += 5;
				total_time_mid = total_time_right;
				total_time_right = 0;
			}
			else //if (right > 25)
				break;
		}
		else //if (total_time_mid < total_time_right)
			break;
	}

	//Step 2: determining the upper bound of the backoff range
	if (BCL::rank() == MASTER_UNIT)
		std::cout << "STEP 2: determining the upper bound of the backoff range\n\n";

	total_time_array = new double [right + 1];
	for (i = 0; i < right + 1; ++i)
		total_time_array[i] = 0;
	total_time_array[mid] = total_time_mid;
	total_time_array[right] = total_time_right;
	mid = right;
	while (true)
	{
		if (total_time_array[mid - 1] == 0)
		{
			my_stack = new stack<uint32_t>(ELEMS_PER_UNIT / 2);
			total_time_array[mid - 1] = work_time(my_stack, num_ops, left, mid - 1);
			delete my_stack;
		}

		if (BCL::rank() == MASTER_UNIT)
		{
			std::cout << "mid = " << mid << std::endl;
			std::cout << "array[" << mid - 1 << "] = " << total_time_array[mid - 1] <<
				", array[" << mid << "] = " << total_time_array[mid] << "\n\n";
		}

		if (total_time_mid < total_time_array[mid] ||
				total_time_array[mid - 1] < total_time_array[mid])
		{
			--mid;
			if (mid == left)
				break;
		}
		else //others
			break;
	}

	total_time_right = total_time_array[mid];
	delete [] total_time_array;
	right = mid;

	//Step 3: determining the lower bound of the backoff range
	if (BCL::rank() == MASTER_UNIT)
		std::cout << "STEP 3: determining the lower bound of the backoff range\n\n";

	if (left < right)
	{
		total_time_array = new double [right + 1];
        	for (i = 0; i < right + 1; ++i)
                	total_time_array[i] = 0;
		total_time_array[left] = total_time_right;
		mid = left;
		while (true)
		{
			if (total_time_array[mid - 1] == 0)
			{
				my_stack = new stack<uint32_t>(ELEMS_PER_UNIT / 2);
				total_time_array[mid - 1] = work_time(my_stack, num_ops, mid - 1, right);
				delete my_stack;
			}

			if (mid < right && total_time_array[mid + 1] == 0)
			{
				my_stack = new stack<uint32_t>(ELEMS_PER_UNIT / 2);
				total_time_array[mid + 1] = work_time(my_stack, num_ops, mid + 1, right);
				delete my_stack;
			}

			if (BCL::rank() == MASTER_UNIT)
			{
				std::cout << "mid = " << mid << std::endl;
				std::cout << "array[" << mid - 1 << "] = " << total_time_array[mid - 1] <<
					", array[" << mid << "] = " << total_time_array[mid] <<
					", array[" << mid + 1 << "] = " << total_time_array[mid + 1] << "\n\n";
			}

			if (total_time_array[mid - 1] < total_time_array[mid] &&
					total_time_array[mid - 1] < total_time_array[mid + 1])
			{
				--mid;
				if (mid == 0)
					break;
			}
			else if (total_time_array[mid + 1] < total_time_array[mid] &&
					total_time_array[mid + 1] < total_time_array[mid - 1])
			{
				++mid;
				if (mid == right)
					break;
			}
			else //others
				break;
		}
		total_time_mid = total_time_array[mid];
        	delete [] total_time_array;
		left = mid;
	}
	else //if (left == right)
		std::cout << "Do nothing!\n";

        if (BCL::rank() == MASTER_UNIT)
        {
		std::cout << "BACKOFF RANGE = [" << left << ", " << right << "]\n";
		std::cout << "*********************************************************\n";
		std::cout << "*\tBENCHMARK\t:\tSequential-alternating\t*\n";
		std::cout << "*\tNUM_UNITS\t:\t" << BCL::nprocs() << "\t\t\t*\n";
		std::cout << "*\tNUM_OPS\t\t:\t" << num_ops << " (ops/unit)\t\t*\n";
		std::cout << "*\tWORKLOAD\t:\t" << WORKLOAD << " (us)\t\t\t*\n";
		std::cout << "*\tSTACK\t\t:\t" << stack_name.c_str() << "\t\t\t*\n";
		std::cout << "*\tEXEC_TIME\t:\t" << total_time_mid << " (s)\t\t*\n";
		std::cout << "*\tTHROUGHPUT\t:\t" << ELEMS_PER_UNIT / total_time_mid << " (ops/s)\t\t*\n";
		std::cout << "*********************************************************\n";
        }

	BCL::finalize();

	return 0;
}

template <typename T>
inline double work_time(stack<T> *my_stack, const uint64_t &num_ops, const uint32_t &left, const uint32_t &right)
{
        uint32_t        i,
                        value;
        double		start,
			end,
			elapsed_time,
			total_time;

	bk_init = exp2l(left);
	bk_max = exp2l(right);

	start = MPI_Wtime();

	for (i = 0; i < num_ops / 2; ++i)
	{
		//debugging
		#ifdef DEBUGGING
			printf ("[%lu]%u\n", BCL::rank(), i);
		#endif

		my_stack->push(i);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));

                my_stack->pop(value);
		std::this_thread::sleep_for(std::chrono::microseconds(WORKLOAD));
	}

	end = MPI_Wtime();

	elapsed_time = (end - start) - ((double) num_ops * WORKLOAD) / 1000000;

	total_time = BCL::allreduce(elapsed_time, BCL::max<double>{});

	return total_time;
}
