#pragma once

/**
 * Main difference between those and OpaaxMathConst
 *
 * Here we privileged hexadecimal value instead of decimal value.
 * And it's a good reminder for decimal to hexa.
 */

#pragma region UInt
/*------------------------ UINT32 ----------------------------*/
#define OP_UMAX_32 0xFFFF'FFFF

/*------------------------ UINT32 ----------------------------*/
#define OP_UMAX_64 0xFFFF'FFFF'FFFF
#pragma endregion UInt


/*---------------------------------------------------------
 * Int
 * /!\ Not UINT /!\
 * Int MyInt = OP_HUNDRED_16();
 * UInt16 = OP_HUNDRED_16();
 *
 * Both are not equals.
 *--------------------------------------------------------*/
#pragma region Int
/*------------------------ INT8 ----------------------------*/
#define OP_MINUS_ONE_8  0xFF
#define OP_HUNDRED_8    0x64
#define OP_ONE_8        0x01
#define OP_INT8_MAX     0x7F //127
#define OP_INT8_MIN     0x80 //-128

/*------------------------ INT16 ----------------------------*/
#define OP_MINUS_ONE_16 0xFFFF
#define OP_HUNDRED_16   0x0064
#define OP_ONE_16       0x0001
#define OP_INT16_MAX    0x7FFF
#define OP_INT16_MIN    0x8000

/*------------------------ INT32 ----------------------------*/
#define OP_ZERO_32      0x0000'0000
#define OP_MINUS_ONE_32 0xFFFF'FFFF
#define OP_HUNDRED_32   0x0000'0064
#define OP_ONE_32       0x0000'0001
#define OP_INT32_MAX    0x7FFF'FFFF //2'147'483'647
#define OP_INT32_MIN    0x8000'0000 // -2'147'483'648

/*------------------------ Int64 ----------------------------*/
#define OP_ZERO_64      0x0000'0000'0000'0000
#define OP_MINUS_ONE_64 0xFFFF'FFFF'FFFF'FFFF
#define OP_HUNDRED_64   0x0000'0000'0000'0064
#define OP_ONE_64       0x0000'0000'0000'0001
#define OP_INT64_MAX    0x7FFF'FFFF'FFFF'FFFF
#define OP_INT64_MIN    0x8000'0000'0000'0000
#pragma endregion //Int

/*------------------------ Int Compute ----------------------------*/
#define OP_DIVIDE_INT_BY_2(x) x >> 1
#define OP_MULTIPLY_INT_BY_2(x) x << 1
