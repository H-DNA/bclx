#pragma once

#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>

namespace bclx
{

template<typename T>
inline void store(const T *src, const gptr<T> &dst, const size_t &size)
{
	lwrite(src, dst, size);
}

template<typename T>
inline void store(const T &src, const gptr<T> &dst)
{
	lwrite(&src, dst, 1);
}

template<typename T, typename U>
inline void rput_sync(const std::vector<T> &src, const gptr<U> &dst, const U &type)
{
	rwrite_sync(src, dst, type);
}

template<typename T>
inline void rput_sync(const T *src, const gptr<T> &dst, const size_t &size)
{
	rwrite_sync(src, dst, size);
}

template<typename T>
inline void rput_sync(const T &src, const gptr<T> &dst)
{
	rwrite_sync(&src, dst, 1);
}

template<typename T, typename U>
inline void rput_async(const std::vector<T> &src, const gptr<U> &dst, const U &type)
{
	rwrite_async(src, dst, type);
}

template<typename T>
inline void rput_async(const T *src, const gptr<T> &dst, const size_t &size)
{
	rwrite_async(src, dst, size);
}

template<typename T>
inline void rput_async(const T &src, const gptr<T> &dst)
{
	rwrite_async(&src, dst, 1);
}

template<typename T>
inline void rput_block(const T *src, const gptr<T> &dst, const size_t &size)
{
	rwrite_block(src, dst, size);
}

template<typename T>
inline void rput_block(const T &src, const gptr<T> &dst)
{
	rwrite_block(&src, dst, 1);
}

template<typename T>
inline void rput_block2(const T *src, const gptr<T> &dst, const size_t &size)
{
	rwrite_block2(src, dst, size);
}

template<typename T>
inline void rput_block2(const T &src, const gptr<T> &dst)
{
	rwrite_block2(&src, dst, 1);
}

template<typename T>
inline void aput_sync(const T &src, const gptr<T> &dst)
{
	awrite_sync(&src, dst, 1);
}

template<typename T>
inline void aput_async(const T &src, const gptr<T> &dst)
{
	awrite_async(&src, dst, 1);
}

template<typename T>
inline void load(const gptr<T> &src, T *dst, const size_t &size)
{
	lread(src, dst, size);
}

template<typename T>
inline T load(const gptr<T> &src)
{
	T rv;
	lread(src, &rv, 1);
	return rv;
}

template<typename T, typename U>
inline void rget_sync(const gptr<U> &src, std::vector<T> &dst, const U &type)
{
        rread_sync(src, dst, type);
}

template<typename T>
inline void rget_sync(const gptr<T> &src, T *dst, const size_t &size)
{
	rread_sync(src, dst, size);
}

template<typename T>
inline T rget_sync(const gptr<T> &src)
{
	T rv;
	rread_sync(src, &rv, 1);
	return rv;
}

template<typename T>
inline T rget_async(const gptr<T> &src)
{
	T rv;
	rread_async(src, &rv, 1);
	return rv;
}

template<typename T>
inline T aget_sync(const gptr<T> &src)
{
	T rv;
	aread_sync(src, &rv, 1);
	return rv;
}

template<typename T>
inline T aget_async(const gptr<T> &src)
{
	T rv;
	aread_async(src, &rv, 1);
	return rv;
}

template<typename T, typename U>
inline T fao_sync(const gptr<T> &dst, const T &val, const BCL::atomic_op<U> &op)
{
	T rv;
	fetch_and_op_sync(dst, &val, op, &rv);
	return rv;
}

template<typename T>
inline T cas_sync(const gptr<T> &dst, const T &old_val, const T &new_val)
{
	T rv;
	compare_and_swap_sync(dst, &old_val, &new_val, &rv);
	return rv;
}

template<typename T>
inline T swap_sync(const gptr<T> &dst, const T &old_val, const T &new_val)
{
	T rv;
	fetch_and_set_sync(dst, &old_val, &new_val, &rv);
	return rv;
}

template<typename T>
inline T scatter(const T *src_buf, const size_t &src_rank)
{
	T rv;
	scatter(src_buf, &rv, src_rank, 1);
	return rv;
}

template<typename T>
inline void alltoall(const T *src_buf, T *dst_buf)
{
	alltoall(src_buf, dst_buf, 1);
}

template<typename T, typename U>
inline T reduce(const T &src_buf, const size_t &dst_rank, const BCL::atomic_op<U> &op)
{
	T rv;
	reduce(&src_buf, &rv, dst_rank, op, 1);
	return rv;
}

template<typename T, typename U>
inline T allreduce(const T &src_buf, const BCL::atomic_op<U> &op)
{
	T rv;
	allreduce(&src_buf, &rv, op, 1);
	return rv;
}

template<typename T>
inline void send(const T& src_buf, const size_t &dst_rank)
{
	send(&src_buf, dst_rank, 1);
}

template<typename T>
inline void recv(T& dst_buf, const size_t &src_rank)
{
	recv(&dst_buf, src_rank, 1);
}

} /* namespace bclx */
