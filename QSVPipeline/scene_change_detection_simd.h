﻿// -----------------------------------------------------------------------------------------
// QSVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2011-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// -------------------------------------------------------------------------------------------

#pragma once

#include <emmintrin.h> //SSE2
#include <smmintrin.h> //SSE4.1
#include <nmmintrin.h> //SSE4.2
#include <cstdint>
#include "rgy_simd.h"
#include "scene_change_detection.h"

void make_hist_sse2(const uint8_t *frame_Y, hist_t *hist_buf, int y_start, int y_end, int y_step, int x_skip, int width, int pitch);
void make_hist_sse41_popcnt(const uint8_t *frame_Y, hist_t *hist_buf, int y_start, int y_end, int y_step, int x_skip, int width, int pitch);
void make_hist_avx(const uint8_t *frame_Y, hist_t *hist_buf, int y_start, int y_end, int y_step, int x_skip, int width, int pitch);
void make_hist_avx2(const uint8_t *frame_Y, hist_t *hist_buf, int y_start, int y_end, int y_step, int x_skip, int width, int pitch);


static inline int popcnt32(uint32_t bits) {
    bits = (bits & 0x55555555) + (bits >> 1 & 0x55555555);
    bits = (bits & 0x33333333) + (bits >> 2 & 0x33333333);
    bits = (bits & 0x0f0f0f0f) + (bits >> 4 & 0x0f0f0f0f);
    bits = (bits & 0x00ff00ff) + (bits >> 8 & 0x00ff00ff);
    bits = (bits & 0x0000ffff) + (bits >>16 & 0x0000ffff);
    return bits;
}
static inline int popcnt64(uint64_t bits) {
    bits = (bits & 0x5555555555555555) + ((bits >> 1) & 0x5555555555555555);
    bits = (bits & 0x3333333333333333) + ((bits >> 2) & 0x3333333333333333);
    bits = (bits & 0x0f0f0f0f0f0f0f0f) + ((bits >> 4) & 0x0f0f0f0f0f0f0f0f);
    bits = (bits & 0x00ff00ff00ff00ff) + ((bits >> 8) & 0x00ff00ff00ff00ff);
    bits = (bits & 0x0000ffff0000ffff) + ((bits >>16) & 0x0000ffff0000ffff);
    bits = (bits & 0x00000000ffffffff) + ((bits >>32) & 0x00000000ffffffff);
    return (int)bits;
}

static uint32_t __forceinline get_count(__m128i x0, __m128i x1, __m128i xComp, const uint32_t simd) {
    __m128i x2 = _mm_cmpeq_epi8(x0, xComp);
    __m128i x3 = _mm_cmpeq_epi8(x1, xComp);
    uint32_t count0 = _mm_movemask_epi8(x2);
    uint32_t count1 = _mm_movemask_epi8(x3);
    uint32_t count = ((count1 << 16) | count0);
    return (simd & POPCNT) ? _mm_popcnt_u32(count) : popcnt32(count);
}

#if !defined(_M_X64)
#pragma warning (push)
#pragma warning (disable: 4799)
static __m128i __forceinline set_m64x2_to_m128i(__m64 hi, __m64 lo) {
    __m128i x1 = _mm_movpi64_epi64(hi);
    __m128i x0 = _mm_movpi64_epi64(lo);
    return _mm_or_si128(x0, _mm_slli_si128(x1, 8));
}
#pragma warning (pop)
#endif

static void __forceinline make_hist_simd(const uint8_t *frame_Y, hist_t *hist_buf, int y_start, int y_end, int y_step, int x_skip, int width, int pitch, const uint32_t simd) {
    __m128i x0, x1, x2, x3;
    __m128i xOne = _mm_set1_epi8(1);
    __m128i xMask = _mm_set1_epi8(0xff>>HIST_LEN_2N);
    alignas(16) hist_t tmp;
    _mm_store_si128((__m128i*)((uint8_t *)&tmp +  0), _mm_setzero_si128());
    _mm_store_si128((__m128i*)((uint8_t *)&tmp + 16), _mm_setzero_si128());
    _mm_store_si128((__m128i*)((uint8_t *)&tmp + 32), _mm_setzero_si128());
    _mm_store_si128((__m128i*)((uint8_t *)&tmp + 48), _mm_setzero_si128());

    for (int y = y_start; y < y_end; y += y_step) {
        const uint8_t *ptr = (uint8_t *)((size_t)(frame_Y + y * pitch + x_skip + 63) & ~63);
        const uint8_t *ptr_fin = ptr + (width & ~31) - x_skip;
        __m128i xComp = _mm_setzero_si128();
#if _M_X64 //よくわからんがx64ではMMXが使えないらしい
        __m128i xStepBuf = _mm_setzero_si128();
        struct alignas(16) hist16_t {
            USHORT u[16];
        } line_sum;
        _mm_store_si128((__m128i*)((uint8_t *)&line_sum +  0), xComp);
        _mm_store_si128((__m128i*)((uint8_t *)&line_sum + 16), xComp);

        for ( ; ptr < ptr_fin; ptr += 32) {
            xComp = _mm_xor_si128(xComp, xComp);
            x0 = (simd & SSE41) ? _mm_stream_load_si128((__m128i*)(ptr +  0)) : _mm_load_si128((__m128i*)(ptr +  0));
            x1 = (simd & SSE41) ? _mm_stream_load_si128((__m128i*)(ptr + 16)) : _mm_load_si128((__m128i*)(ptr + 16));

            x0 = _mm_srli_epi64(x0, HIST_LEN_2N);
            x1 = _mm_srli_epi64(x1, HIST_LEN_2N);

            x0 = _mm_and_si128(x0, xMask);
            x1 = _mm_and_si128(x1, xMask);

            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 3);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 4);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 5);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 6);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 7);
            xComp = _mm_add_epi8(xComp, xOne);

            xStepBuf = _mm_adds_epu16(xStepBuf, _mm_load_si128((__m128i*)((uint8_t *)&line_sum +  0)));
            _mm_store_si128((__m128i*)((uint8_t *)&line_sum +  0), xStepBuf);
            
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 3);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 4);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 5);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 6);
            xComp = _mm_add_epi8(xComp, xOne);
            xStepBuf = _mm_insert_epi16(xStepBuf, get_count(x0, x1, xComp, simd), 7);
            xComp = _mm_add_epi8(xComp, xOne);

            xStepBuf = _mm_adds_epu16(xStepBuf, _mm_load_si128((__m128i*)((uint8_t *)&line_sum + 16)));
            _mm_store_si128((__m128i*)((uint8_t *)&line_sum + 16), xStepBuf);
        }
        //末端はスキップ
        //ptr_fin = ptr + (width & 31);
        //for ( ; ptr < ptr_fin; ptr += 2) {
        //    line_sum.u[ptr[0]>>HIST_LEN_2N]++;
        //    line_sum.u[ptr[1]>>HIST_LEN_2N]++;
        //}
        x1 = _mm_load_si128((__m128i*)((uint8_t *)&line_sum +  0));
        x3 = _mm_load_si128((__m128i*)((uint8_t *)&line_sum + 16));
#else
        __m64 m4 = _mm_setzero_si64();
        __m64 m5 = _mm_setzero_si64();
        __m64 m6 = _mm_setzero_si64();
        __m64 m7 = _mm_setzero_si64();

        for ( ; ptr < ptr_fin; ptr += 32) {
            __m64 m0 = _mm_setzero_si64();
            __m64 m1 = _mm_setzero_si64();
            __m64 m2 = _mm_setzero_si64();
            __m64 m3 = _mm_setzero_si64();

            xComp = _mm_xor_si128(xComp, xComp);
            x0 = (simd & SSE41) ? _mm_stream_load_si128((__m128i*)(ptr +  0)) : _mm_load_si128((__m128i*)(ptr +  0));
            x1 = (simd & SSE41) ? _mm_stream_load_si128((__m128i*)(ptr + 16)) : _mm_load_si128((__m128i*)(ptr + 16));

            x0 = _mm_srli_epi64(x0, HIST_LEN_2N);
            x1 = _mm_srli_epi64(x1, HIST_LEN_2N);

            x0 = _mm_and_si128(x0, xMask);
            x1 = _mm_and_si128(x1, xMask);
            
            m0 = _mm_insert_pi16(m0, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            m0 = _mm_insert_pi16(m0, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            m0 = _mm_insert_pi16(m0, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            m0 = _mm_insert_pi16(m0, get_count(x0, x1, xComp, simd), 3);
            xComp = _mm_add_epi8(xComp, xOne);

            m1 = _mm_insert_pi16(m1, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            m1 = _mm_insert_pi16(m1, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            m1 = _mm_insert_pi16(m1, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            m1 = _mm_insert_pi16(m1, get_count(x0, x1, xComp, simd), 3);
            xComp = _mm_add_epi8(xComp, xOne);

            m2 = _mm_insert_pi16(m2, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            m2 = _mm_insert_pi16(m2, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            m2 = _mm_insert_pi16(m2, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            m2 = _mm_insert_pi16(m2, get_count(x0, x1, xComp, simd), 3);
            xComp = _mm_add_epi8(xComp, xOne);
            
            m3 = _mm_insert_pi16(m3, get_count(x0, x1, xComp, simd), 0);
            xComp = _mm_add_epi8(xComp, xOne);
            m3 = _mm_insert_pi16(m3, get_count(x0, x1, xComp, simd), 1);
            xComp = _mm_add_epi8(xComp, xOne);
            m3 = _mm_insert_pi16(m3, get_count(x0, x1, xComp, simd), 2);
            xComp = _mm_add_epi8(xComp, xOne);
            m3 = _mm_insert_pi16(m3, get_count(x0, x1, xComp, simd), 3);

            m4 = _mm_adds_pi16(m4, m0);
            m5 = _mm_adds_pi16(m5, m1);
            m6 = _mm_adds_pi16(m6, m2);
            m7 = _mm_adds_pi16(m7, m3);
        }
        //末端はスキップ
        //ptr_fin = ptr + (width & 31);
        //for ( ; ptr < ptr_fin; ptr += 2) {
        //    line_sum.u[ptr[0]>>HIST_LEN_2N]++;
        //    line_sum.u[ptr[1]>>HIST_LEN_2N]++;
        //}
        x1 = set_m64x2_to_m128i(m5, m4);
        x3 = set_m64x2_to_m128i(m7, m6);
#endif
        x0 = _mm_unpacklo_epi16(x1, _mm_setzero_si128());
        x1 = _mm_unpackhi_epi16(x1, _mm_setzero_si128());
        x2 = _mm_unpacklo_epi16(x3, _mm_setzero_si128());
        x3 = _mm_unpackhi_epi16(x3, _mm_setzero_si128());
        x0 = _mm_add_epi32(x0, _mm_load_si128((__m128i*)((uint8_t *)&tmp +  0)));
        x1 = _mm_add_epi32(x1, _mm_load_si128((__m128i*)((uint8_t *)&tmp + 16)));
        x2 = _mm_add_epi32(x2, _mm_load_si128((__m128i*)((uint8_t *)&tmp + 32)));
        x3 = _mm_add_epi32(x3, _mm_load_si128((__m128i*)((uint8_t *)&tmp + 48)));
        _mm_store_si128((__m128i*)((uint8_t *)&tmp +  0), x0);
        _mm_store_si128((__m128i*)((uint8_t *)&tmp + 16), x1);
        _mm_store_si128((__m128i*)((uint8_t *)&tmp + 32), x2);
        _mm_store_si128((__m128i*)((uint8_t *)&tmp + 48), x3);
    }
    _mm_store_si128((__m128i*)((uint8_t *)hist_buf +  0), _mm_load_si128((__m128i *)((uint8_t *)&tmp +  0)));
    _mm_store_si128((__m128i*)((uint8_t *)hist_buf + 16), _mm_load_si128((__m128i *)((uint8_t *)&tmp + 16)));
    _mm_store_si128((__m128i*)((uint8_t *)hist_buf + 32), _mm_load_si128((__m128i *)((uint8_t *)&tmp + 32)));
    _mm_store_si128((__m128i*)((uint8_t *)hist_buf + 48), _mm_load_si128((__m128i *)((uint8_t *)&tmp + 48)));
#if !_M_X64
    _mm_empty();
#endif
}
