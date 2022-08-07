#pragma once

namespace bclx
{

class timer
{
public:
	timer();
	~timer();
	void start();
	void stop();
	double get();
	void reset();

private:
	double	time_start;
	double	time_elapsed;
};

} /* namespace bclx */

bclx::timer::timer()
	: time_start{0}, time_elapsed{0} {}

bclx::timer::~timer() {}

void bclx::timer::start()
{
	time_elapsed = MPI_Wtime();
}

void bclx::timer::stop()
{
	time_elapsed += MPI_Wtime() - time_start;
}

double bclx::timer::get()
{
	return time_elapsed;
}

void bclx::timer::reset()
{
	time_start = 0;
	time_elapsed = 0;
}

