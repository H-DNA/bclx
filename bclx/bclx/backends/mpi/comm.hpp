#pragma once

namespace bclx
{

template<typename T>
inline void lwrite(const T *src, const gptr<T> &dst, const size_t &size)
{
    	std::memcpy(dst.local(), src, size*sizeof(T));
}

template<typename T>
inline void rwrite_sync(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Put(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, BCL::win);
	MPI_Win_flush(dst.rank, BCL::win);
}

template<typename T, typename U>
inline void rwrite_sync(const std::vector<T> &src, const gptr<U> &dst, const U &type)
{
	MPI_Put(src.data(), src.size()*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, 1, type.get(), BCL::win);
	MPI_Win_flush(dst.rank, BCL::win);
}

template<typename T>
inline void rwrite_async(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Put(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, BCL::win);
}

template<typename T, typename U>
inline void rwrite_async(const std::vector<T> &src, const gptr<U> &dst, const U &type)
{
	MPI_Put(src.data(), src.size()*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, 1, type.get(), BCL::win);
}

template<typename T>
inline void rwrite_block(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Put(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, BCL::win);
	MPI_Win_flush_local(dst.rank, BCL::win);
}

template<typename T>
inline void rwrite_block2(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Request	request;
	MPI_Status	status;
	MPI_Rput(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, BCL::win, &request);
	MPI_Wait(&request, &status);
}

template<typename T>
inline void awrite_sync(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Accumulate(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, MPI_REPLACE, BCL::win);
	MPI_Win_flush(dst.rank, BCL::win);
}

template<typename T>
inline void awrite_async(const T *src, const gptr<T> &dst, const size_t &size)
{
	MPI_Accumulate(src, size*sizeof(T), MPI_CHAR, dst.rank, dst.ptr, size*sizeof(T), MPI_CHAR, MPI_REPLACE, BCL::win);
}

template<typename T>
inline void lread(const gptr<T> &src, T *dst, const size_t &size)
{
    	std::memcpy(dst, src.local(), size*sizeof(T));
}

template<typename T>
inline void rread_sync(const gptr<T> &src, T *dst, const size_t &size)
{
	MPI_Get(dst, size*sizeof(T), MPI_CHAR, src.rank, src.ptr, size*sizeof(T), MPI_CHAR, BCL::win);
	MPI_Win_flush(src.rank, BCL::win);
}

template<typename T>
inline void rread_async(const gptr<T> &src, T *dst, const size_t &size)
{
	MPI_Get(dst, size*sizeof(T), MPI_CHAR, src.rank, src.ptr, size*sizeof(T), MPI_CHAR, BCL::win);
}

template<typename T>
inline void aread_sync(const gptr<T> &src, T *dst, const size_t &size)
{
	T *origin_addr;
	MPI_Get_accumulate(origin_addr, 0, MPI_CHAR, dst, size*sizeof(T), MPI_CHAR,
				src.rank, src.ptr, size*sizeof(T), MPI_CHAR, MPI_NO_OP, BCL::win);
	MPI_Win_flush(src.rank, BCL::win);
}

template<typename T>
inline void aread_async(const gptr<T> &src, T *dst, const size_t &size)
{
	T *origin_addr;
	MPI_Get_accumulate(origin_addr, 0, MPI_CHAR, dst, size*sizeof(T), MPI_CHAR,
				src.rank, src.ptr, size*sizeof(T), MPI_CHAR, MPI_NO_OP, BCL::win);
}

template<typename T, typename U>
inline void fetch_and_op_sync(const gptr<T> &dst, const T *val, const BCL::atomic_op<U> &op, T *result)
{
	MPI_Fetch_and_op(val, result, op.type(), dst.rank, dst.ptr, op.op(), BCL::win);
	MPI_Win_flush(dst.rank, BCL::win);
}

template<typename T>
inline void compare_and_swap_sync(const gptr<T> &dst, const T *old_val, const T *new_val, T *result)
{
	MPI_Datatype datatype;

	if (sizeof(T) == 8)
		datatype = MPI_UINT64_T;
	else if (sizeof(T) == 4)
		datatype = MPI_UINT32_T;
	else if (sizeof(T) == 1)
	{
        	if (typeid(T) == typeid(bool))
                	datatype = MPI_C_BOOL;
		else if (typeid(T) == typeid(uint8_t))
			datatype = MPI_UINT8_T;
		else
			printf("ERROR: The datatype not found!\n");
	}
	else
		printf("ERROR: The datatype not found!\n");

	MPI_Compare_and_swap(new_val, old_val, result, datatype, dst.rank, dst.ptr, BCL::win);
	MPI_Win_flush(dst.rank, BCL::win);
}

inline void barrier_sync()
{
	MPI_Win_flush_all(BCL::win);
	MPI_Barrier(BCL::comm);
}

inline void barrier_async()
{
	MPI_Barrier(BCL::comm);
}

template<typename T>
inline void scatter(const T *src_buf, T *dst_buf, const size_t &src_rank, const size_t &size)
{
	MPI_Scatter(src_buf, size * sizeof(T), MPI_CHAR,
			dst_buf, size * sizeof(T), MPI_CHAR, src_rank, BCL::comm);
}

template<typename T>
inline void alltoall(const T *src_buf, T *dst_buf, const size_t &size)
{
	MPI_Alltoall(src_buf, size * sizeof(T), MPI_CHAR,
			dst_buf, size * sizeof(T), MPI_CHAR, BCL::comm);
}

template<typename T, typename U>
inline void reduce(const T *src_buf, T *dst_buf, const size_t &dst_rank, const BCL::atomic_op<U> &op, const size_t &size)
{
	MPI_Reduce(src_buf, dst_buf, size, op.type(), op.op(), dst_rank, BCL::comm);
}

template<typename T, typename U>
inline void allreduce(const T *src_buf, T *dst_buf, const BCL::atomic_op<U> &op, const size_t &size)
{
	MPI_Allreduce(src_buf, dst_buf, size, op.type(), op.op(), BCL::comm);
}

template<typename T>
inline void send(const T *src_buf, const size_t &dst_rank, const size_t &size)
{
	MPI_Send(src_buf, size*sizeof(T), MPI_CHAR, dst_rank, 0, BCL::comm);
}

template<typename T>
inline void recv(T *dst_buf, const size_t &src_rank, const size_t &size)
{
	MPI_Status status;
	MPI_Recv(dst_buf, size*sizeof(T), MPI_CHAR, src_rank, 0, BCL::comm, &status);
}

} /* namespace bclx */

