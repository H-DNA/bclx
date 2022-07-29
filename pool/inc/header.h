#ifndef HEADER_H
#define HEADER_H

template<typename T>
struct block
{
	uint64_t	header;
	T		data;
};

#endif /* HEADER_H */
