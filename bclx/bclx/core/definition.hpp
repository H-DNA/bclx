#pragma once

#include <bcl/bcl.hpp>
#include <cstdint>

namespace bclx {

template <typename T> using gptr = BCL::GlobalPtr<T>;

const uint64_t MASTER_UNIT = 0;

} /* namespace bclx */
