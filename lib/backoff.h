#ifndef BACKOFF_H
#define BACKOFF_H

#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include "../config.h"

namespace backoff
{

        class backoff
        {
        public:
                backoff(const uint64_t &init, const uint64_t &max);
                uint64_t delay_exp();
		uint64_t delay_dbl();
		uint64_t delay_inc();

        private:
		uint64_t	bk;
		uint64_t	bk_max;
		uint64_t	res;
	};

} /* namespace backoff */

backoff::backoff::backoff(const uint64_t &init, const uint64_t &max)
{
        bk = init;
	bk_max = max;
}

uint64_t backoff::backoff::delay_exp()
{
	if (bk > bk_max)
		bk = bk_max;

        std::default_random_engine generator;
        std::uniform_int_distribution<uint64_t> distribution(0, bk);
	res = distribution(generator);
        std::this_thread::sleep_for(std::chrono::microseconds(res));

	if (2 * bk <= bk_max)
		bk *= 2;

	return res;
}

uint64_t backoff::backoff::delay_dbl()
{
	if (bk > bk_max)
		bk = bk_max;

	res = bk;
        std::this_thread::sleep_for(std::chrono::microseconds(bk));
	
	if (2 * bk <= bk_max)
		bk *= 2;

	return res;
}

uint64_t backoff::backoff::delay_inc()
{
	if (bk > bk_max)
		bk = bk_max;

	res = bk;
        std::this_thread::sleep_for(std::chrono::microseconds(bk));

	if (1 + bk <= bk_max)
		++bk;

	return res;
}

#endif /* BACKOFF_H */
