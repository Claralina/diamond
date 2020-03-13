/****
DIAMOND protein aligner
Copyright (C) 2013-2019 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#include "score_vector.h"

#ifndef SCORE_VECTOR_INT16_H_
#define SCORE_VECTOR_INT16_H_

#ifdef __SSE2__

template<>
struct score_vector<int16_t>
{

	score_vector() :
		data_(_mm_set1_epi16(SHRT_MIN))
	{}

	explicit score_vector(int x)
	{
		data_ = _mm_set1_epi16(x);
	}

	explicit score_vector(int16_t x)
	{
		data_ = _mm_set1_epi16(x);
	}

	explicit score_vector(__m128i data) :
		data_(data)
	{ }
	
	explicit score_vector(const int16_t *x):
		data_(_mm_loadu_si128((const __m128i*)x))
	{}

	explicit score_vector(const uint16_t *x) :
		data_(_mm_loadu_si128((const __m128i*)x))
	{}

	score_vector(int x, const Saturated&) :
		data_(_mm_set1_epi16(short(SHRT_MIN + x)))
	{}

	score_vector(unsigned a, uint64_t seq)
	{
		const uint16_t* row((uint16_t*)&score_matrix.matrix16()[a << 5]);
		uint64_t b = uint64_t(row[seq & 0xff]);
		seq >>= 8;
		b |= uint64_t(row[seq & 0xff]) << 16;
		seq >>= 8;
		b |= uint64_t(row[seq & 0xff]) << 16 * 2;
		seq >>= 8;
		b |= uint64_t(row[seq & 0xff]) << 16 * 3;
		seq >>= 8;
		uint64_t c = uint64_t(row[seq & 0xff]);
		seq >>= 8;
		c |= uint64_t(row[seq & 0xff]) << 16;
		seq >>= 8;
		c |= uint64_t(row[seq & 0xff]) << 16 * 2;
		seq >>= 8;
		c |= uint64_t(row[seq & 0xff]) << 16 * 3;
		data_ = _mm_set_epi64x(c, b);
	}

	score_vector(unsigned a, const __m128i &seq, const score_vector &bias)
	{
#ifdef __SSSE3__
		const __m128i *row = reinterpret_cast<const __m128i*>(&score_matrix.matrix8u()[a << 5]);

		__m128i high_mask = _mm_slli_epi16(_mm_and_si128(seq, _mm_set1_epi8('\x10')), 3);
		__m128i seq_low = _mm_or_si128(seq, high_mask);
		__m128i seq_high = _mm_or_si128(seq, _mm_xor_si128(high_mask, _mm_set1_epi8('\x80')));

		__m128i r1 = _mm_load_si128(row);
		__m128i r2 = _mm_load_si128(row + 1);
		__m128i s1 = _mm_shuffle_epi8(r1, seq_low);
		__m128i s2 = _mm_shuffle_epi8(r2, seq_high);
		data_ = _mm_subs_epi16(_mm_and_si128(_mm_or_si128(s1, s2), _mm_set1_epi16(255)), bias.data_);
#endif
	}

	score_vector operator+(const score_vector &rhs) const
	{
		return score_vector(_mm_adds_epi16(data_, rhs.data_));
	}

	score_vector operator-(const score_vector &rhs) const
	{
		return score_vector(_mm_subs_epi16(data_, rhs.data_));
	}

	score_vector& operator-=(const score_vector &rhs)
	{
		data_ = _mm_subs_epi16(data_, rhs.data_);
		return *this;
	}

	score_vector& operator &=(const score_vector& rhs) {
		data_ = _mm_and_si128(data_, rhs.data_);
		return *this;
	}

	score_vector& operator++() {
		data_ = _mm_adds_epi16(data_, _mm_set1_epi16(1));
		return *this;
	}

	score_vector& max(const score_vector &rhs)
	{
		data_ = _mm_max_epi16(data_, rhs.data_);
		return *this;
	}

	friend score_vector max(const score_vector& lhs, const score_vector &rhs)
	{
		return score_vector(_mm_max_epi16(lhs.data_, rhs.data_));
	}

	uint16_t cmpeq(const score_vector &rhs) const
	{
		return _mm_movemask_epi8(_mm_cmpeq_epi16(data_, rhs.data_));
	}

	__m128i cmpgt(const score_vector &rhs) const
	{
		return _mm_cmpgt_epi16(data_, rhs.data_);
	}

	void store(int16_t *ptr) const
	{
		_mm_storeu_si128((__m128i*)ptr, data_);
	}

	int16_t operator[](int i) const {
		int16_t d[8];
		store(d);
		return d[i];
	}

	void set(int i, int16_t x) {
		int16_t d[8];
		store(d);
		d[i] = x;
		data_ = _mm_loadu_si128((__m128i*)d);
	}

	__m128i data_;

};

template<>
struct ScoreTraits<score_vector<int16_t>>
{
	enum { CHANNELS = 8, BITS = 16 };
	typedef int16_t Score;
	typedef uint16_t Unsigned;
	static score_vector<int16_t> zero()
	{
		return score_vector<int16_t>();
	}
	static void saturate(score_vector<int16_t> &v)
	{
	}
	static constexpr int16_t zero_score()
	{
		return SHRT_MIN;
	}
	static int int_score(Score s)
	{
		return (uint16_t)s ^ 0x8000;
	}
	static constexpr int16_t max_score()
	{
		return SHRT_MAX;
	}
	static constexpr int max_int_score() {
		return SHRT_MAX - SHRT_MIN;
	}
};

static inline score_vector<int16_t> load_sv(const int16_t *x) {
	return score_vector<int16_t>(x);
}

static inline score_vector<int16_t> load_sv(const uint16_t *x) {
	return score_vector<int16_t>(x);
}

#endif

#endif