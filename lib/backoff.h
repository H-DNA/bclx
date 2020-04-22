#ifndef BACKOFF_H
#define BACKOFF_H

#include <unistd.h>
#include <random>
#include <cmath>

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
                uint32_t        c;
	};

} /* namespace backoff */

backoff::backoff::backoff()
{
        c = 0;
}

void backoff::backoff::delay_exp()
{
	++c;
        std::default_random_engine generator;
        std::uniform_int_distribution<uint32_t> distribution(0, c);
        usleep(exp2(distribution(generator)) - 1);
}

void backoff::backoff::delay_dbl()
{
        usleep(exp2(c));
	++c;
}

void backoff::backoff::delay_inc()
{
	++c;
	usleep(c);
}

#endif /* BACKOFF_H */
