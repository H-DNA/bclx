#ifndef PGAS_H
#define PGAS_H

#include <iostream>

namespace pgas
{
	/* Macros */
	template <typename T>
	using atomic_op = BCL::atomic_op<T>;

	template <typename T>
	using replace = BCL::replace<T>;

	/* Constants */
	enum mode {SYNC, ASYNC, BLOCK};

	/* 64-bit global pointer */
	struct gptr64
	{
		uint64_t	rank	: 24;
		uint64_t 	offs	: 40;
	};

	/* N-bit global pointer */
	template <typename T>
	struct gptr
	{
		gptr64		ptr;
		uint64_t	base;

					gptr();
					gptr(const std::nullptr_t &null);
		BCL::GlobalPtr<T>	convert() const;
		gptr<T>			operator=(const std::nullptr_t &null);
		gptr<T>			operator=(const gptr<T> &p);
		bool 			operator==(const std::nullptr_t &null);
		bool 			operator==(const gptr<T> &p);
		bool 			operator!=(const std::nullptr_t &null);
		bool 			operator!=(const gptr<T> &p);
	};

	/* Enviroment management functions */
	inline void init();

	const auto finalize = BCL::finalize;

	inline uint64_t my_uid();

	inline uint64_t num_units();

	template <typename T, typename U>
	inline gptr<U> convert(const gptr<T> &p, const int64_t	&disp = 0);

	/* Global memory management functions */
	template <typename T>
	inline gptr<T> malloc(const uint64_t &size);

	template <typename T>
	inline void free(const gptr<T> &addr);

	/* Communicative functions */
	template <typename T>
	inline T load(const gptr<T> &src);

	template <typename T>
	inline void store(const T &src, const gptr<T> &dst);

	template <typename T>
	inline T rget(const gptr<T> &src, const mode &mod = SYNC);

	template <typename T>
	inline void rput(const T &src, const gptr<T> &dst, const mode &mod = SYNC);

	template <typename T>
	inline T aget(const gptr<T> &src, const mode &mod = SYNC);

	template <typename T>
	inline void aput(const T &src, const gptr<T> &dst, const mode &mod = SYNC);

        template <typename T, typename U>
        inline T fao(const gptr<T> &ptr, const T &val, const atomic_op<U> &op, const mode &mod = SYNC);

	template <typename T>
	inline T cas(const gptr<T> &ptr, const T &oldVal, const T &newVal, const mode &mod = SYNC);

	/* Synchronous functions */
	inline void barrier(const mode &mod = SYNC);

} /* namespace pgas */

template <typename T>
pgas::gptr<T>::gptr()
{
	//do nothing
}

template <typename T>
pgas::gptr<T>::gptr(const std::nullptr_t &null)
{
	this->ptr = {0, 0};
	this->base = 0;
}

template <typename T>
BCL::GlobalPtr<T> pgas::gptr<T>::convert() const
{
        BCL::GlobalPtr<T>	gptr_bcl;

        gptr_bcl.rank = this->ptr.rank;
	gptr_bcl.ptr = this->base + this->ptr.offs * sizeof(T);

	return gptr_bcl;
}

template <typename T>
pgas::gptr<T> pgas::gptr<T>::operator=(const std::nullptr_t &null)
{
	*this = {{0, 0}, 0};

	return *this;
}

template <typename T>
pgas::gptr<T> pgas::gptr<T>::operator=(const gptr<T> &p)
{
	this->ptr = p.ptr;
	this->base = p.base;

	return *this;
}

template <typename T>
bool pgas::gptr<T>::operator==(const std::nullptr_t &null)
{
	return operator==(gptr(nullptr));
}

template <typename T>
bool pgas::gptr<T>::operator==(const gptr<T> &p)
{
	return (this->ptr.rank == p.ptr.rank && this->ptr.offs == p.ptr.offs && this->base == p.base);
}

template <typename T>
bool pgas::gptr<T>::operator!=(const std::nullptr_t &null)
{
	return operator!=(gptr(nullptr));
}

template <typename T>
bool pgas::gptr<T>::operator!=(const gptr<T> &p)
{
	return !operator==(p);
}

inline void pgas::init()
{
	BCL::init();
}

inline uint64_t pgas::my_uid()
{
	return BCL::rank();
}

inline uint64_t pgas::num_units()
{
	return BCL::nprocs();
}

template <typename T, typename U>
inline pgas::gptr<U> pgas::convert(const pgas::gptr<T> &p, const int64_t &disp)
{
	pgas::gptr<U> 	temp;

	temp.ptr = {p.ptr.rank, 0};
	temp.base = p.base + p.ptr.offs * sizeof(T) + disp;

	return temp;
}

template <typename T>
inline pgas::gptr<T> pgas::malloc(const uint64_t &size)
{
	BCL::GlobalPtr<T>	gptr_bcl;
	pgas::gptr<T>		gptr_dds;

	gptr_bcl = BCL::alloc<T>(size);
	gptr_dds.ptr = {gptr_bcl.rank, 0};
	gptr_dds.base = gptr_bcl.ptr;

	return gptr_dds;
}

template <typename T>
inline void pgas::free(const gptr<T> &addr)
{
	BCL::GlobalPtr<T>	gptr_bcl;

	gptr_bcl = addr.convert();
	BCL::dealloc<T>(gptr_bcl);
}

template <typename T>
inline T pgas::load(const pgas::gptr<T> &src)
{
	T 			dst;
	BCL::GlobalPtr<T>	gptr_bcl;

	gptr_bcl = src.convert();
	std::memcpy(&dst, gptr_bcl.local(), sizeof(T));

	return dst;
}

template <typename T>
inline void pgas::store(const T &src, const pgas::gptr<T> &dst)
{
	BCL::GlobalPtr<T>	gptr_bcl;

	gptr_bcl = dst.convert();
	std::memcpy(gptr_bcl.local(), &src, sizeof(T));
}

template <typename T>
inline T pgas::rget(const pgas::gptr<T> &src, const pgas::mode &mod)
{
        T                       dst;
        BCL::GlobalPtr<T>       gptr_bcl;

	gptr_bcl = src.convert();
	MPI_Get(&dst, sizeof(T), MPI_BYTE, gptr_bcl.rank, gptr_bcl.ptr, sizeof(T), MPI_BYTE, BCL::win);

        if (mod == pgas::SYNC)
                MPI_Win_flush(gptr_bcl.rank, BCL::win);

	return dst;
}

template <typename T>
inline void pgas::rput(const T &src, const pgas::gptr<T> &dst, const pgas::mode &mod)
{
        BCL::GlobalPtr<T>       gptr_bcl;

	gptr_bcl = dst.convert();
	MPI_Put(&src, sizeof(T), MPI_BYTE, gptr_bcl.rank, gptr_bcl.ptr, sizeof(T), MPI_BYTE, BCL::win);

        if (mod == pgas::SYNC)
                MPI_Win_flush(gptr_bcl.rank, BCL::win);
}

template <typename T>
inline T pgas::aget(const pgas::gptr<T> &src, const pgas::mode &mod)
{
        T                       origin,
				dst;
        BCL::GlobalPtr<T>       gptr_bcl;

        gptr_bcl = src.convert();
	MPI_Get_accumulate(&origin, 0, MPI_BYTE, &dst, sizeof(T), MPI_BYTE,
				gptr_bcl.rank, gptr_bcl.ptr, sizeof(T), MPI_BYTE, MPI_NO_OP, BCL::win);

        if (mod == pgas::SYNC)
                MPI_Win_flush(gptr_bcl.rank, BCL::win);

	return dst;
}

template <typename T>
inline void pgas::aput(const T &src, const pgas::gptr<T> &dst, const pgas::mode &mod)
{
        BCL::GlobalPtr<T>       gptr_bcl;

        gptr_bcl = dst.convert();
	MPI_Accumulate(&src, sizeof(T), MPI_BYTE,
				gptr_bcl.rank, gptr_bcl.ptr, sizeof(T), MPI_BYTE, MPI_REPLACE, BCL::win);

        if (mod == pgas::SYNC)
                MPI_Win_flush(gptr_bcl.rank, BCL::win);
}

template <typename T, typename U>
inline T pgas::fao(const pgas::gptr<T> &ptr, const T &val, const pgas::atomic_op<U> &op, const pgas::mode &mod)
{
        BCL::GlobalPtr<T>       gptr_bcl;
        T                       res;

	gptr_bcl = ptr.convert();
	MPI_Fetch_and_op(&val, &res, op.type(), gptr_bcl.rank, gptr_bcl.ptr, op.op(), BCL::win);

	if (mod == pgas::SYNC)
		MPI_Win_flush(gptr_bcl.rank, BCL::win);

	return res;
}

template <typename T>
inline T pgas::cas(const pgas::gptr<T> &ptr, const T &oldVal, const T &newVal, const pgas::mode &mod)
{
	MPI_Datatype		datatype;
	BCL::GlobalPtr<T>	gptr_bcl;
	T 			res;

	if (sizeof(T) == 8)
		datatype = MPI_UINT64_T;
	else if (sizeof(T) == 4)
		datatype = MPI_UINT32_T;
	else if (sizeof(T) == 1)
	{
		if (typeid(T) == typeid(bool))
			datatype = MPI_C_BOOL;
		else
			std::cout << "ERROR: the datatype not found!" << std::endl;
	}
	else
		std::cout << "ERROR: the datatype not found!" << std::endl;

	gptr_bcl = ptr.convert();
	MPI_Compare_and_swap(&newVal, &oldVal, &res, datatype, gptr_bcl.rank, gptr_bcl.ptr, BCL::win);

	if (mod == pgas::SYNC)
		MPI_Win_flush(gptr_bcl.rank, BCL::win);

	return res;
}

inline void pgas::barrier(const pgas::mode &mod)
{
	if (mod == pgas::SYNC)
		MPI_Win_flush_all(BCL::win);

	MPI_Barrier(BCL::comm);
}

#endif /* PGAS_H */
