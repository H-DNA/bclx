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
                void delay_exp();
		void delay_dbl();
		void delay_inc();

        private:
		uint64_t	bk;
		uint64_t	bk_max;
	};

} /* namespace backoff */

backoff::backoff::backoff(const uint64_t &init, const uint64_t &max)
{
        bk = init;
	bk_max = max;
}

void backoff::backoff::delay_exp()
{
	if (bk > bk_max)
		bk = bk_max;

        std::default_random_engine generator;
        std::uniform_int_distribution<uint64_t> distribution(0, bk);
        std::this_thread::sleep_for(std::chrono::microseconds(distribution(generator)));

	if (2 * bk <= bk_max)
		bk *= 2;
}

void backoff::backoff::delay_dbl()
{
	if (bk > bk_max)
		bk = bk_max;

        std::this_thread::sleep_for(std::chrono::microseconds((uint64_t) bk));
	
	if (2 * bk <= bk_max)
		bk *= 2;
}

void backoff::backoff::delay_inc()
{
	if (bk > bk_max)
		bk = bk_max;

        std::this_thread::sleep_for(std::chrono::microseconds(bk));

	if (1 + bk <= bk_max)
		++bk;

}

#endif /* BACKOFF_H */
