#ifndef BACKOFF_H
#define BACKOFF_H

#include <unistd.h>
#include <random>
#include <cmath>
#include "../config.h"

namespace backoff
{

        class backoff
        {
        public:
                backoff(const uint64_t &th);
                void delay_exp();
		uint64_t delay_dbl();
		void delay_inc();

        private:
		uint64_t	c;
		uint64_t	bk_th;
	};

} /* namespace backoff */

backoff::backoff::backoff(const uint64_t &th)
{
        c = 0;
	bk_th = th;
}

void backoff::backoff::delay_exp()
{
	++c;
	if (exp2l(c) > bk_th)
		c = 1;
        std::default_random_engine generator;
        std::uniform_int_distribution<uint64_t> distribution(0, exp2l(c) - 1);
        usleep(distribution(generator));
}

uint64_t backoff::backoff::delay_dbl()
{
        usleep(exp2l(c));
	++c;
	if (exp2l(c) > bk_th)
		c = 0;

	return (uint64_t) exp2l(c);
}

void backoff::backoff::delay_inc()
{
	++c;
	if (c > bk_th)
		c = 1;
	usleep(c);
}

#endif /* BACKOFF_H */
