#ifndef CONFIG_H
#define CONFIG_H

#include <string>	// std::string...
#include <cmath>	// exp2l...

namespace dds
{

	/* Configurations */
	#define	TRACING
	#define	MEM_DANG5
	//#define	DEBUGGING

	const uint64_t	TOTAL_OPS	=	exp2l(15);
	const uint32_t	WORKLOAD	=	1;		//us
	const uint32_t	TSS_INTERVAL	=	1;		//us
	const uint32_t  MASTER_UNIT     =       0;

	/* Aliases */
	template<typename T>
	using gptr = BCL::GlobalPtr<T>;

        /* Constants */
	const bool	EMPTY		= 	false;
	const bool	NON_EMPTY	= 	true;

	/* Varriables */
	std::string	stack_name;
	std::string	mem_manager;
	uint64_t	bk_init		=	exp2l(1);	//us
	uint64_t	bk_max		=	exp2l(20);	//us
	uint64_t	bk_init_master	=	exp2l(1);	//us
	uint64_t	bk_max_master	=	exp2l(20);	//us
	
	// tracing
	#ifdef  TRACING
        	uint64_t	succ_cs		= 0;
		uint64_t	succ_ea 	= 0;
		uint64_t	fail_cs 	= 0;
		uint64_t	fail_ea 	= 0;
		double		fail_time	= 0;
		uint64_t	elem_rc		= 0;
		uint64_t	elem_ru		= 0;
	#endif

} /* namespace dds */

#endif /* CONFIG_H */
