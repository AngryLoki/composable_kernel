#ifndef CK_COMMON_HEADER_HPP
#define CK_COMMON_HEADER_HPP

#include "array.hpp"
#include "array_helper.hpp"
#include "config.hpp"
#include "float_type.hpp"
#include "functional.hpp"
#include "functional2.hpp"
#include "functional3.hpp"
#include "functional4.hpp"
#include "in_memory_operation.hpp"
#include "integral_constant.hpp"
#include "math.hpp"
#include "number.hpp"
#include "sequence.hpp"
#include "sequence_helper.hpp"
#include "synchronization.hpp"
#include "tuple.hpp"
#include "tuple_helper.hpp"
#include "type.hpp"
#include "utility.hpp"

#if CK_USE_AMD_INLINE_ASM
#include "amd_inline_asm.hpp"
#include "amd_llvm_intrinsic.hpp"
#endif

#if CK_USE_AMD_XDLOPS
#include "amd_xdlops.hpp"
#include "amd_xdlops_inline_asm.hpp"
#endif

#endif
