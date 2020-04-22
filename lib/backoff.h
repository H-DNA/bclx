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
		const uint32_t	BK_TH = dds::BK_TH;
                uint32_t        c;
	};

} /* namespace backoff */

backoff::backoff::backoff()
{
        c = 0;
}

void backoff::backoff::delay_exp()
{
	if (exp2(c) < BK_TH)
		++c;
        std::default_random_engine generator;
        std::uniform_int_distribution<uint32_t> distribution(0, c);
        usleep(exp2(distribution(generator)) - 1);
}

void backoff::backoff::delay_dbl()
{
	uint32_t temp = exp2(c);
        usleep(temp);
	if (temp < BK_TH)
		++c;
}

void backoff::backoff::delay_inc()
{
	if (c < BK_TH)
		++c;
	usleep(c);
}

#endif /* BACKOFF_H */
