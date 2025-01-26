///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              01.26.2025
///
/// @file              WasmVm.h
///
/// @brief             Infrastructure to running WASM-compiled programs in a
///                    virtual machine.
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef WASM_VM_H
#define WASM_VM_H

// Custom includes
#include "VirtualMemory.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @enum WasmOpcode
///
/// @brief Enumeration of WASM base instruction opcodes.
typedef enum WasmOpcode {
  // Control Instructions
  WASM_OP_UNREACHABLE          = 0x00,
  WASM_OP_NOP                  = 0x01,
  WASM_OP_BLOCK                = 0x02,
  WASM_OP_LOOP                 = 0x03,
  WASM_OP_IF                   = 0x04,
  WASM_OP_ELSE                 = 0x05,
  WASM_OP_END                  = 0x0B,
  WASM_OP_BR                   = 0x0C,
  WASM_OP_BR_IF                = 0x0D,
  WASM_OP_BR_TABLE             = 0x0E,
  WASM_OP_RETURN               = 0x0F,
  WASM_OP_CALL                 = 0x10,
  WASM_OP_CALL_INDIRECT        = 0x11,
  
  // Parametric Instructions
  WASM_OP_DROP                 = 0x1A,
  WASM_OP_SELECT               = 0x1B,
  
  // Variable Instructions
  WASM_OP_LOCAL_GET            = 0x20,
  WASM_OP_LOCAL_SET            = 0x21,
  WASM_OP_LOCAL_TEE            = 0x22,
  WASM_OP_GLOBAL_GET           = 0x23,
  WASM_OP_GLOBAL_SET           = 0x24,
  
  // Memory Instructions
  WASM_OP_I32_LOAD             = 0x28,
  WASM_OP_I64_LOAD             = 0x29,
  WASM_OP_F32_LOAD             = 0x2A,
  WASM_OP_F64_LOAD             = 0x2B,
  WASM_OP_I32_LOAD8_S          = 0x2C,
  WASM_OP_I32_LOAD8_U          = 0x2D,
  WASM_OP_I32_LOAD16_S         = 0x2E,
  WASM_OP_I32_LOAD16_U         = 0x2F,
  WASM_OP_I64_LOAD8_S          = 0x30,
  WASM_OP_I64_LOAD8_U          = 0x31,
  WASM_OP_I64_LOAD16_S         = 0x32,
  WASM_OP_I64_LOAD16_U         = 0x33,
  WASM_OP_I64_LOAD32_S         = 0x34,
  WASM_OP_I64_LOAD32_U         = 0x35,
  WASM_OP_I32_STORE            = 0x36,
  WASM_OP_I64_STORE            = 0x37,
  WASM_OP_F32_STORE            = 0x38,
  WASM_OP_F64_STORE            = 0x39,
  WASM_OP_I32_STORE8           = 0x3A,
  WASM_OP_I32_STORE16          = 0x3B,
  WASM_OP_I64_STORE8           = 0x3C,
  WASM_OP_I64_STORE16          = 0x3D,
  WASM_OP_I64_STORE32          = 0x3E,
  WASM_OP_MEMORY_SIZE          = 0x3F,
  WASM_OP_MEMORY_GROW          = 0x40,
  
  // Numeric Instructions
  WASM_OP_I32_CONST            = 0x41,
  WASM_OP_I64_CONST            = 0x42,
  WASM_OP_F32_CONST            = 0x43,
  WASM_OP_F64_CONST            = 0x44,
  
  // Comparison Instructions
  WASM_OP_I32_EQZ              = 0x45,
  WASM_OP_I32_EQ               = 0x46,
  WASM_OP_I32_NE               = 0x47,
  WASM_OP_I32_LT_S             = 0x48,
  WASM_OP_I32_LT_U             = 0x49,
  WASM_OP_I32_GT_S             = 0x4A,
  WASM_OP_I32_GT_U             = 0x4B,
  WASM_OP_I32_LE_S             = 0x4C,
  WASM_OP_I32_LE_U             = 0x4D,
  WASM_OP_I32_GE_S             = 0x4E,
  WASM_OP_I32_GE_U             = 0x4F,
  
  WASM_OP_I64_EQZ              = 0x50,
  WASM_OP_I64_EQ               = 0x51,
  WASM_OP_I64_NE               = 0x52,
  WASM_OP_I64_LT_S             = 0x53,
  WASM_OP_I64_LT_U             = 0x54,
  WASM_OP_I64_GT_S             = 0x55,
  WASM_OP_I64_GT_U             = 0x56,
  WASM_OP_I64_LE_S             = 0x57,
  WASM_OP_I64_LE_U             = 0x58,
  WASM_OP_I64_GE_S             = 0x59,
  WASM_OP_I64_GE_U             = 0x5A,
  
  WASM_OP_F32_EQ               = 0x5B,
  WASM_OP_F32_NE               = 0x5C,
  WASM_OP_F32_LT               = 0x5D,
  WASM_OP_F32_GT               = 0x5E,
  WASM_OP_F32_LE               = 0x5F,
  WASM_OP_F32_GE               = 0x60,
  
  WASM_OP_F64_EQ               = 0x61,
  WASM_OP_F64_NE               = 0x62,
  WASM_OP_F64_LT               = 0x63,
  WASM_OP_F64_GT               = 0x64,
  WASM_OP_F64_LE               = 0x65,
  WASM_OP_F64_GE               = 0x66,
  
  // Arithmetic Instructions
  WASM_OP_I32_CLZ              = 0x67,
  WASM_OP_I32_CTZ              = 0x68,
  WASM_OP_I32_POPCNT           = 0x69,
  WASM_OP_I32_ADD              = 0x6A,
  WASM_OP_I32_SUB              = 0x6B,
  WASM_OP_I32_MUL              = 0x6C,
  WASM_OP_I32_DIV_S            = 0x6D,
  WASM_OP_I32_DIV_U            = 0x6E,
  WASM_OP_I32_REM_S            = 0x6F,
  WASM_OP_I32_REM_U            = 0x70,
  WASM_OP_I32_AND              = 0x71,
  WASM_OP_I32_OR               = 0x72,
  WASM_OP_I32_XOR              = 0x73,
  WASM_OP_I32_SHL              = 0x74,
  WASM_OP_I32_SHR_S            = 0x75,
  WASM_OP_I32_SHR_U            = 0x76,
  WASM_OP_I32_ROTL             = 0x77,
  WASM_OP_I32_ROTR             = 0x78,
  
  WASM_OP_I64_CLZ              = 0x79,
  WASM_OP_I64_CTZ              = 0x7A,
  WASM_OP_I64_POPCNT           = 0x7B,
  WASM_OP_I64_ADD              = 0x7C,
  WASM_OP_I64_SUB              = 0x7D,
  WASM_OP_I64_MUL              = 0x7E,
  WASM_OP_I64_DIV_S            = 0x7F,
  WASM_OP_I64_DIV_U            = 0x80,
  WASM_OP_I64_REM_S            = 0x81,
  WASM_OP_I64_REM_U            = 0x82,
  WASM_OP_I64_AND              = 0x83,
  WASM_OP_I64_OR               = 0x84,
  WASM_OP_I64_XOR              = 0x85,
  WASM_OP_I64_SHL              = 0x86,
  WASM_OP_I64_SHR_S            = 0x87,
  WASM_OP_I64_SHR_U            = 0x88,
  WASM_OP_I64_ROTL             = 0x89,
  WASM_OP_I64_ROTR             = 0x8A,
  
  WASM_OP_F32_ABS              = 0x8B,
  WASM_OP_F32_NEG              = 0x8C,
  WASM_OP_F32_CEIL             = 0x8D,
  WASM_OP_F32_FLOOR            = 0x8E,
  WASM_OP_F32_TRUNC            = 0x8F,
  WASM_OP_F32_NEAREST          = 0x90,
  WASM_OP_F32_SQRT             = 0x91,
  WASM_OP_F32_ADD              = 0x92,
  WASM_OP_F32_SUB              = 0x93,
  WASM_OP_F32_MUL              = 0x94,
  WASM_OP_F32_DIV              = 0x95,
  WASM_OP_F32_MIN              = 0x96,
  WASM_OP_F32_MAX              = 0x97,
  WASM_OP_F32_COPYSIGN         = 0x98,
  
  WASM_OP_F64_ABS              = 0x99,
  WASM_OP_F64_NEG              = 0x9A,
  WASM_OP_F64_CEIL             = 0x9B,
  WASM_OP_F64_FLOOR            = 0x9C,
  WASM_OP_F64_TRUNC            = 0x9D,
  WASM_OP_F64_NEAREST          = 0x9E,
  WASM_OP_F64_SQRT             = 0x9F,
  WASM_OP_F64_ADD              = 0xA0,
  WASM_OP_F64_SUB              = 0xA1,
  WASM_OP_F64_MUL              = 0xA2,
  WASM_OP_F64_DIV              = 0xA3,
  WASM_OP_F64_MIN              = 0xA4,
  WASM_OP_F64_MAX              = 0xA5,
  WASM_OP_F64_COPYSIGN         = 0xA6,
  
  // Conversion Instructions
  WASM_OP_I32_WRAP_I64         = 0xA7,
  WASM_OP_I32_TRUNC_F32_S      = 0xA8,
  WASM_OP_I32_TRUNC_F32_U      = 0xA9,
  WASM_OP_I32_TRUNC_F64_S      = 0xAA,
  WASM_OP_I32_TRUNC_F64_U      = 0xAB,
  WASM_OP_I64_EXTEND_I32_S     = 0xAC,
  WASM_OP_I64_EXTEND_I32_U     = 0xAD,
  WASM_OP_I64_TRUNC_F32_S      = 0xAE,
  WASM_OP_I64_TRUNC_F32_U      = 0xAF,
  WASM_OP_I64_TRUNC_F64_S      = 0xB0,
  WASM_OP_I64_TRUNC_F64_U      = 0xB1,
  WASM_OP_F32_CONVERT_I32_S    = 0xB2,
  WASM_OP_F32_CONVERT_I32_U    = 0xB3,
  WASM_OP_F32_CONVERT_I64_S    = 0xB4,
  WASM_OP_F32_CONVERT_I64_U    = 0xB5,
  WASM_OP_F32_DEMOTE_F64       = 0xB6,
  WASM_OP_F64_CONVERT_I32_S    = 0xB7,
  WASM_OP_F64_CONVERT_I32_U    = 0xB8,
  WASM_OP_F64_CONVERT_I64_S    = 0xB9,
  WASM_OP_F64_CONVERT_I64_U    = 0xBA,
  WASM_OP_F64_PROMOTE_F32      = 0xBB,
  
  // Reinterpretation Instructions
  WASM_OP_I32_REINTERPRET_F32  = 0xBC,
  WASM_OP_I64_REINTERPRET_F64  = 0xBD,
  WASM_OP_F32_REINTERPRET_I32  = 0xBE,
  WASM_OP_F64_REINTERPRET_I64  = 0xBF,
  
  // Garbage Collection Instructions (0xFB prefix)
  WASM_OP_GARBAGE_COLLECTION   = 0xFB,
  
  // Extended Instructions (0xFC prefix)
  WASM_OP_EXTEND               = 0xFC,
  
  // Vector Instructions (0xFD prefix)
  WASM_OP_VECTOR               = 0xFD
} WasmOpcode;

/// @enum WasmGarbageCollectionOpcode
///
/// @brief Enumeration of WASM garbage collection instruction opcodes.
typedef enum WasmGarbageCollectionOpcode {
  // Type Instructions
  WASM_OP_STRUCT_NEW           = 0x01,
  WASM_OP_STRUCT_NEW_DEFAULT   = 0x02,
  WASM_OP_STRUCT_GET           = 0x03,
  WASM_OP_STRUCT_GET_S         = 0x04,
  WASM_OP_STRUCT_GET_U         = 0x05,
  WASM_OP_STRUCT_SET           = 0x06,
  
  // Array Instructions
  WASM_OP_ARRAY_NEW            = 0x07,
  WASM_OP_ARRAY_NEW_DEFAULT    = 0x08,
  WASM_OP_ARRAY_GET            = 0x09,
  WASM_OP_ARRAY_GET_S          = 0x0A,
  WASM_OP_ARRAY_GET_U          = 0x0B,
  WASM_OP_ARRAY_SET            = 0x0C,
  WASM_OP_ARRAY_LEN            = 0x0D,

  // Ref Instructions  
  WASM_OP_REF_NULL             = 0x0E,
  WASM_OP_REF_IS_NULL          = 0x0F,
  WASM_OP_REF_FUNC             = 0x10,
  WASM_OP_REF_AS_NON_NULL      = 0x11,
  WASM_OP_BR_ON_NULL           = 0x12,
  WASM_OP_BR_ON_NON_NULL       = 0x13,
  
  // Cast Instructions
  WASM_OP_REF_TEST             = 0x14,
  WASM_OP_REF_CAST             = 0x15,
  WASM_OP_BR_ON_CAST           = 0x16,
  WASM_OP_BR_ON_CAST_FAIL      = 0x17,

  // External Type Instructions
  WASM_OP_ANY_CONVERT_EXTERN   = 0x18,
  WASM_OP_EXTERN_CONVERT_ANY   = 0x19
} WasmGarbageCollectionOpcode;

/// @enum WasmExtendedOpcode
///
/// @brief Enumeration of WASM extended instruction opcodes.
typedef enum WasmExtendedOpcode {
  // Saturating Truncation Instructions (0xFC prefix)
  WASM_OP_I32_TRUNC_SAT_F32_S  = 0x00,
  WASM_OP_I32_TRUNC_SAT_F32_U  = 0x01,
  WASM_OP_I32_TRUNC_SAT_F64_S  = 0x02,
  WASM_OP_I32_TRUNC_SAT_F64_U  = 0x03,
  WASM_OP_I64_TRUNC_SAT_F32_S  = 0x04,
  WASM_OP_I64_TRUNC_SAT_F32_U  = 0x05,
  WASM_OP_I64_TRUNC_SAT_F64_S  = 0x06,
  WASM_OP_I64_TRUNC_SAT_F64_U  = 0x07,
  
  // Memory Instructions (0xFC prefix)
  WASM_OP_MEMORY_INIT          = 0x08,
  WASM_OP_DATA_DROP            = 0x09,
  WASM_OP_MEMORY_COPY          = 0x0A,
  WASM_OP_MEMORY_FILL          = 0x0B,
  
  // Table Instructions (0xFC prefix)
  WASM_OP_TABLE_INIT           = 0x0C,
  WASM_OP_ELEM_DROP            = 0x0D,
  WASM_OP_TABLE_COPY           = 0x0E,
  WASM_OP_TABLE_GROW           = 0x0F,
  WASM_OP_TABLE_SIZE           = 0x10,
  WASM_OP_TABLE_FILL           = 0x11
} WasmExtendedOpcode;

/// @enum WasmVectorOpcode
///
/// @brief Enumeration of WASM vector instruction opcodes.
typedef enum WasmVectorOpcode {
  // v128 memory instructions
  WASM_OP_V128_LOAD            = 0x00,
  WASM_OP_V128_LOAD8X8_S       = 0x01,
  WASM_OP_V128_LOAD8X8_U       = 0x02,
  WASM_OP_V128_LOAD16X4_S      = 0x03,
  WASM_OP_V128_LOAD16X4_U      = 0x04,
  WASM_OP_V128_LOAD32X2_S      = 0x05,
  WASM_OP_V128_LOAD32X2_U      = 0x06,
  WASM_OP_V128_LOAD8_SPLAT     = 0x07,
  WASM_OP_V128_LOAD16_SPLAT    = 0x08,
  WASM_OP_V128_LOAD32_SPLAT    = 0x09,
  WASM_OP_V128_LOAD64_SPLAT    = 0x0A,
  WASM_OP_V128_STORE           = 0x0B,

  // v128 constants
  WASM_OP_V128_CONST           = 0x0C,
  
  // v128 shuffles
  WASM_OP_I8X16_SHUFFLE        = 0x0D,
  WASM_OP_I8X16_SWIZZLE        = 0x0E,
  
  // v128 lane operations  
  WASM_OP_I8X16_SPLAT          = 0x0F,
  WASM_OP_I16X8_SPLAT          = 0x10,
  WASM_OP_I32X4_SPLAT          = 0x11,
  WASM_OP_I64X2_SPLAT          = 0x12,
  WASM_OP_F32X4_SPLAT          = 0x13,
  WASM_OP_F64X2_SPLAT          = 0x14,

  // i8x16 lane operations
  WASM_OP_I8X16_EXTRACT_LANE_S = 0x15,
  WASM_OP_I8X16_EXTRACT_LANE_U = 0x16,
  WASM_OP_I8X16_REPLACE_LANE   = 0x17,
  
  // i16x8 lane operations
  WASM_OP_I16X8_EXTRACT_LANE_S = 0x18,
  WASM_OP_I16X8_EXTRACT_LANE_U = 0x19,
  WASM_OP_I16X8_REPLACE_LANE   = 0x1A,
  
  // i32x4 lane operations
  WASM_OP_I32X4_EXTRACT_LANE   = 0x1B,
  WASM_OP_I32X4_REPLACE_LANE   = 0x1C,
  
  // i64x2 lane operations
  WASM_OP_I64X2_EXTRACT_LANE   = 0x1D,
  WASM_OP_I64X2_REPLACE_LANE   = 0x1E,
  
  // f32x4 lane operations
  WASM_OP_F32X4_EXTRACT_LANE   = 0x1F,
  WASM_OP_F32X4_REPLACE_LANE   = 0x20,
  
  // f64x2 lane operations
  WASM_OP_F64X2_EXTRACT_LANE   = 0x21,
  WASM_OP_F64X2_REPLACE_LANE   = 0x22,

  // i8x16 comparison operations
  WASM_OP_I8X16_EQ             = 0x23,
  WASM_OP_I8X16_NE             = 0x24,
  WASM_OP_I8X16_LT_S           = 0x25,
  WASM_OP_I8X16_LT_U           = 0x26,
  WASM_OP_I8X16_GT_S           = 0x27,
  WASM_OP_I8X16_GT_U           = 0x28,
  WASM_OP_I8X16_LE_S           = 0x29,
  WASM_OP_I8X16_LE_U           = 0x2A,
  WASM_OP_I8X16_GE_S           = 0x2B,
  WASM_OP_I8X16_GE_U           = 0x2C,

  // i16x8 comparison operations
  WASM_OP_I16X8_EQ             = 0x2D,
  WASM_OP_I16X8_NE             = 0x2E,
  WASM_OP_I16X8_LT_S           = 0x2F,
  WASM_OP_I16X8_LT_U           = 0x30,
  WASM_OP_I16X8_GT_S           = 0x31,
  WASM_OP_I16X8_GT_U           = 0x32,
  WASM_OP_I16X8_LE_S           = 0x33,
  WASM_OP_I16X8_LE_U           = 0x34,
  WASM_OP_I16X8_GE_S           = 0x35,
  WASM_OP_I16X8_GE_U           = 0x36,

  // i32x4 comparison operations
  WASM_OP_I32X4_EQ             = 0x37,
  WASM_OP_I32X4_NE             = 0x38,
  WASM_OP_I32X4_LT_S           = 0x39,
  WASM_OP_I32X4_LT_U           = 0x3A,
  WASM_OP_I32X4_GT_S           = 0x3B,
  WASM_OP_I32X4_GT_U           = 0x3C,
  WASM_OP_I32X4_LE_S           = 0x3D,
  WASM_OP_I32X4_LE_U           = 0x3E,
  WASM_OP_I32X4_GE_S           = 0x3F,
  WASM_OP_I32X4_GE_U           = 0x40,

  // f32x4 comparison operations
  WASM_OP_F32X4_EQ             = 0x41,
  WASM_OP_F32X4_NE             = 0x42,
  WASM_OP_F32X4_LT             = 0x43,
  WASM_OP_F32X4_GT             = 0x44,
  WASM_OP_F32X4_LE             = 0x45,
  WASM_OP_F32X4_GE             = 0x46,

  // f64x2 comparison operations
  WASM_OP_F64X2_EQ             = 0x47,
  WASM_OP_F64X2_NE             = 0x48,
  WASM_OP_F64X2_LT             = 0x49,
  WASM_OP_F64X2_GT             = 0x4A,
  WASM_OP_F64X2_LE             = 0x4B,
  WASM_OP_F64X2_GE             = 0x4C,

  // v128 bitwise operations
  WASM_OP_V128_NOT             = 0x4D,
  WASM_OP_V128_AND             = 0x4E,
  WASM_OP_V128_OR              = 0x4F,
  WASM_OP_V128_XOR             = 0x50,
  WASM_OP_V128_BITSELECT       = 0x51,

  // i8x16 arithmetic operations
  WASM_OP_I8X16_ABS            = 0x60,
  WASM_OP_I8X16_NEG            = 0x61,
  WASM_OP_I8X16_ADD            = 0x62,
  WASM_OP_I8X16_SUB            = 0x63,
  WASM_OP_I8X16_MIN_S          = 0x64,
  WASM_OP_I8X16_MIN_U          = 0x65,
  WASM_OP_I8X16_MAX_S          = 0x66,
  WASM_OP_I8X16_MAX_U          = 0x67,
  WASM_OP_I8X16_ADD_SAT_S      = 0x68,
  WASM_OP_I8X16_ADD_SAT_U      = 0x69,
  WASM_OP_I8X16_SUB_SAT_S      = 0x6A,
  WASM_OP_I8X16_SUB_SAT_U      = 0x6B,

  // i16x8 arithmetic operations
  WASM_OP_I16X8_ABS            = 0x80,
  WASM_OP_I16X8_NEG            = 0x81,
  WASM_OP_I16X8_ADD            = 0x82,
  WASM_OP_I16X8_SUB            = 0x83,
  WASM_OP_I16X8_MIN_S          = 0x84,
  WASM_OP_I16X8_MIN_U          = 0x85,
  WASM_OP_I16X8_MAX_S          = 0x86,
  WASM_OP_I16X8_MAX_U          = 0x87,
  WASM_OP_I16X8_ADD_SAT_S      = 0x88,
  WASM_OP_I16X8_ADD_SAT_U      = 0x89,
  WASM_OP_I16X8_SUB_SAT_S      = 0x8A,
  WASM_OP_I16X8_SUB_SAT_U      = 0x8B,
  WASM_OP_I16X8_MUL            = 0x8C,

  // i32x4 arithmetic operations
  WASM_OP_I32X4_ABS            = 0xA0,
  WASM_OP_I32X4_NEG            = 0xA1,
  WASM_OP_I32X4_ADD            = 0xA2,
  WASM_OP_I32X4_SUB            = 0xA3,
  WASM_OP_I32X4_MUL            = 0xA4,
  WASM_OP_I32X4_MIN_S          = 0xA5,
  WASM_OP_I32X4_MIN_U          = 0xA6,
  WASM_OP_I32X4_MAX_S          = 0xA7,
  WASM_OP_I32X4_MAX_U          = 0xA8,

  // i64x2 arithmetic operations
  WASM_OP_I64X2_NEG            = 0xC1,
  WASM_OP_I64X2_ADD            = 0xC2,
  WASM_OP_I64X2_SUB            = 0xC3,
  WASM_OP_I64X2_MUL            = 0xC4,

  // f32x4 arithmetic operations
  WASM_OP_F32X4_ABS            = 0xE0,
  WASM_OP_F32X4_NEG            = 0xE1,
  WASM_OP_F32X4_SQRT           = 0xE2,
  WASM_OP_F32X4_ADD            = 0xE3,
  WASM_OP_F32X4_SUB            = 0xE4,
  WASM_OP_F32X4_MUL            = 0xE5,
  WASM_OP_F32X4_DIV            = 0xE6,
  WASM_OP_F32X4_MIN            = 0xE7,
  WASM_OP_F32X4_MAX            = 0xE8,

  // f64x2 arithmetic operations
  WASM_OP_F64X2_ABS            = 0xEC,
  WASM_OP_F64X2_NEG            = 0xED,
  WASM_OP_F64X2_SQRT           = 0xEE,
  WASM_OP_F64X2_ADD            = 0xEF,
  WASM_OP_F64X2_SUB            = 0xF0,
  WASM_OP_F64X2_MUL            = 0xF1,
  WASM_OP_F64X2_DIV            = 0xF2,
  WASM_OP_F64X2_MIN            = 0xF3,
  WASM_OP_F64X2_MAX            = 0xF4
} WasmVectorOpcode;

/// @struct WasmVm
///
/// @brief State information for a WASM process.
///
/// @param programCounter The offset into the codeSegment of the next
///   instruction to be evaluated.
/// @param codeSegment Virtual memory for the compiled code of the program.
///   Backing for this will be the compiled, on-disk binary.
/// @param linearMemory Virtual memory for the WASM linear memory segment.
///   Backing for this will be an on-disk file with a random filename.
/// @param globalStack Virtual memory for the WASM global stack segement.
///   Backing for this will be an on-disk file with a random filename.
/// @param callStack Virtual memory for the WASM per-function stack segment.
///   Backing for this will be an on-disk file with a random filename.
/// @param globalStorage Virtual memory for the WASM global storage segment.
///   Backing for this will be an on-disk file with a random filename.
/// @param tableSpace Virtual memory for the WASM function table segment.
///   Backing for this will be an on-disk file with a random filename.
typedef struct WasmVm {
  uint32_t           programCounter;
  VirtualMemoryState codeSegment;
  VirtualMemoryState linearMemory;
  VirtualMemoryState globalStack;
  VirtualMemoryState callStack;
  VirtualMemoryState globalStorage;
  VirtualMemoryState tableSpace;
} WasmVm;

/// @typedef WasmImportFunc
///
/// @brief Type of function pointer that can be imported by a WASM program.
typedef int32_t (*WasmImportFunc)(WasmVm*);

/// @struct WasmImport
///
/// @brief Structure of an entry in a WASM import table.
///
/// @param functionName Full name of the function in the format "module.field".
/// @param function WasmImportFunc function pointer to the implementation of
///   the function.
typedef struct WasmImport {
  const char *functionName;
  WasmImportFunc function;
} WasmImport;


void wasmVmCleanup(WasmVm *wasmVm);
int wasmVmInit(WasmVm *wasmVm, const char *programPath);
int32_t wasmStackPush32(VirtualMemoryState *stack, uint32_t value);
int32_t wasmStackPop32(VirtualMemoryState *stack, uint32_t *value);
int32_t wasmStackInit(VirtualMemoryState *stack);
int32_t wasmParseImports(
  WasmVm *wasmVm, const WasmImport *importTable, uint32_t importTableLength);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WASM_VM_H
