#pragma once

#include <bcl/bcl.hpp>
#include <bclx/core/definition.hpp>
#include <mpi.h>

namespace BCL {

struct abstract_bool : public virtual abstract_op<bool> {
  MPI_Datatype type() const { return MPI_C_BOOL; }
};

struct abstract_uint32_t : public virtual abstract_op<uint32_t> {
  MPI_Datatype type() const { return MPI_UINT32_T; }
};

template <>
struct plus<uint32_t> : public abstract_plus<uint32_t>,
                        public abstract_uint32_t,
                        public atomic_op<uint32_t> {};

template <typename T> struct abstract_replace : public virtual abstract_op<T> {
  MPI_Op op() const { return MPI_REPLACE; }
};

template <typename T> struct replace;

template <>
struct replace<bool> : public abstract_replace<bool>,
                       public abstract_bool,
                       public atomic_op<bool> {};

template <>
struct replace<int> : public abstract_replace<int>,
                      public abstract_int,
                      public atomic_op<int> {};

template <>
struct replace<uint64_t> : public abstract_replace<uint64_t>,
                           public abstract_uint64_t,
                           public atomic_op<uint64_t> {};

template <typename T> struct abstract_sum : public virtual abstract_op<T> {
  MPI_Op op() const { return MPI_SUM; }
};

template <typename T> struct sum;

template <>
struct sum<uint64_t> : public abstract_sum<uint64_t>,
                       public abstract_uint64_t,
                       public atomic_op<uint64_t> {};

template <>
struct sum<double> : public abstract_sum<double>,
                     public abstract_double,
                     public atomic_op<double> {};

template <typename T> struct abstract_max : public virtual abstract_op<T> {
  MPI_Op op() const { return MPI_MAX; }
};

template <typename T> struct max;

template <>
struct max<double> : public abstract_max<double>,
                     public abstract_double,
                     public atomic_op<double> {};

} /* namespace BCL */
