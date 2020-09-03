#ifndef BACKOFF2_H
#define BACKOFF2_H

#include <unistd.h>
#include <random>
#include <cmath>
#include "../config.h"

namespace backoff2
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
		uint64_t	bk_init;
		uint64_t	bk_max;
	};

} /* namespace backoff */

backoff2::backoff::backoff(const uint64_t &init, const uint64_t &max)
{
        bk = bk_init = init;
	bk_max = max;
}

void backoff2::backoff::delay_exp()
{
	if (bk > bk_max)
		bk = bk_max;

        std::default_random_engine generator;
        std::uniform_int_distribution<uint64_t> distribution(0, bk);
        usleep(distribution(generator));

	if (2 * bk <= bk_max)
		bk *= 2;
	else //if (2 * bk > bk_max)
		bk = bk_init;

}

void backoff2::backoff::delay_dbl()
{
	if (bk > bk_max)
		bk = bk_max;

        usleep(bk);

	if (2 * bk <= bk_max)
		bk *= 2;
	else //if (2 * bk > bk_max)
		bk = bk_init;
}

void backoff2::backoff::delay_inc()
{
	if (bk > bk_max)
		bk = bk_max;

	usleep(bk);

	if (1 + bk <= bk_max)
		++bk;
	else //if (1 + bk > bk_max)
		bk = bk_init;
}

#endif /* BACKOFF2_H */
