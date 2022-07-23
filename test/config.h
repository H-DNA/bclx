#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>	// uint64_t...
#include <string>	// std::string...
#include <cmath>	// exp2l...

namespace dds
{

	/* Macros */
	//#define	TRACING

	/* Aliases */
	template<typename T>
	using gptr = BCL::GlobalPtr<T>;

        /* Constants */
	const uint64_t	MASTER_UNIT	= 0;
	const uint64_t	TOTAL_OPS	= exp2l(29);

	/* Varriables */
	//std::string	stack_name;
	
	// tracing
	/*#ifdef  TRACING
        	uint64_t	succ_cs		= 0;
	#endif*/

} /* namespace dds */

#endif /* CONFIG_H */
