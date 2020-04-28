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
                backoff();
                void delay_exp();
		void delay_dbl();
		void delay_inc();

        private:
		const uint64_t	BK_TH = dds::BK_TH;
                uint64_t        c;
	};

} /* namespace backoff */

backoff::backoff::backoff()
{
        c = 0;
}

void backoff::backoff::delay_exp()
{
	++c;
	if (exp2l(c) > BK_TH)
		c = 1;
        std::default_random_engine generator;
        std::uniform_int_distribution<uint64_t> distribution(0, exp2l(c) - 1);
        usleep(distribution(generator));
}

void backoff::backoff::delay_dbl()
{
        usleep(exp2l(c));
	++c;
	if (exp2l(c) > BK_TH)
		c = 0;
}

void backoff::backoff::delay_inc()
{
	++c;
	if (c > BK_TH)
		c = 1;
	usleep(c);
}

#endif /* BACKOFF_H */
