/* -*- c++ -*- */
/* 
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "demap_impl.h"

namespace gr {
namespace dvbt_rx {

demap::sptr demap::make() {
	return gnuradio::get_initial_sptr(new demap_impl());
}

/*
 * The private constructor
 */
demap_impl::demap_impl() :
		gr::block("demap",
				gr::io_signature::make2(2, 2,
						sizeof(gr_complex) * config.carriers,
						sizeof(gr_complex) * config.fft_len),
				gr::io_signature::make(1, 1,
						sizeof(char) * config.data_carrier_count)), eqs_scatteredPilotsInverse(
				std::vector<myBuffer_t>(4)) {
	eqs_inBufInverse = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.scattered_pilots_count));
	eqs_outBufInverse = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.scattered_pilots_count));

	eqs_planInverse = fftwf_plan_dft_1d(config.scattered_pilots_count,
			eqs_inBufInverse, eqs_outBufInverse, FFTW_BACKWARD, FFTW_ESTIMATE);

	eqs_inBufForward = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.carriers));
	eqs_outBufForward = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.carriers));

	eqs_planForward = fftwf_plan_dft_1d(config.carriers, eqs_inBufForward,
			eqs_outBufForward, FFTW_FORWARD, FFTW_ESTIMATE);

	for (auto frame { 0 }; frame < 4; frame++) {
		auto tmp = myBufferR_t(config.scattered_pilots_count);
		std::transform(begin(config.scattered_pilots_value[frame]),
				end(config.scattered_pilots_value[frame]), begin(tmp),
				[](myReal_t v) {
					return 1.0f/ v;
				});
		eqs_scatteredPilotsInverse[frame] = myBuffer_t(
				config.scattered_pilots_count);
		auto it = begin(eqs_scatteredPilotsInverse[frame]);
		for (auto c : tmp) {
			*it++ = myComplex_t { c, 0.f };
		}
	}

	ds_dataIdx = std::vector<std::vector<int>>(4);
	for (auto i { 0 }; i < 4; i++) {

		auto tmp = std::vector<int>(config.carriers);

		std::iota(begin(tmp), end(tmp), 0);

		auto without_tps = std::vector<int>();
		std::set_difference(begin(tmp), end(tmp), begin(config.tps),
				end(config.tps), inserter(without_tps, begin(without_tps)));

		auto without_cpilots = std::vector<int>();

		std::set_difference(begin(without_tps), end(without_tps),
				begin(config.continual_pilots), end(config.continual_pilots),
				inserter(without_cpilots, begin(without_cpilots)));

		auto without_spilots = std::vector<int>();

		std::set_difference(begin(without_cpilots), end(without_cpilots),
				begin(config.scattered_pilots[i]),
				end(config.scattered_pilots[i]),
				inserter(without_spilots, begin(without_spilots)));

		ds_dataIdx[i] = without_spilots;
	}

	gpu_deint_init();
}

/*
 * Our virtual destructor.
 */
demap_impl::~demap_impl() {
	fftwf_free(eqs_inBufInverse);
	fftwf_free(eqs_outBufInverse);
	fftwf_destroy_plan(eqs_planInverse);
	fftwf_free(eqs_inBufForward);
	fftwf_free(eqs_outBufForward);
	fftwf_destroy_plan(eqs_planForward);
	gpu_deint_free();
}

// ***************************************************************
// DEMAP START
// ***************************************************************

int demap_impl::frameNum(const myBuffer_t& in) {
	assert(in.size() == config.carriers);
	auto f = myBuffer_t(config.scattered_pilots_count);
	auto maxs = std::vector<myReal_t>(4);

	for (int i { 0 }; i < 4; i++) {
		std::transform(begin(config.scattered_pilots[i]),
				end(config.scattered_pilots[i]), begin(f), [&](myInteger_t a) {
					return in[a];
				});
		auto it = begin(config.scattered_pilots_value[i]);
		auto acc = std::accumulate(begin(f), end(f), myComplex_t { 0, 0 },
				[&](myComplex_t a, myComplex_t b) {
					return a + b * std::conj(*it++);
				});
		maxs[i] = std::abs(acc);
	}

	auto max = std::max_element(begin(maxs), end(maxs));
	auto result = std::distance(begin(maxs), max);
	assert(result >= 0);
	assert(result < 4);

	return result;
}

// ***************************************************************
// DEMAP END
// ***************************************************************

// ***************************************************************
// EQ SCATTERED START
// ***************************************************************

myBuffer_t demap_impl::eqs_selSpilots(const myBuffer_t& in, int frame) {
	assert(in.size() == config.fft_len);

	auto result = myBuffer_t(config.scattered_pilots_count);
	auto i = begin(config.scattered_pilots[frame]);
	std::generate(begin(result), end(result), [&]() {
		return in[config.zeros_left + *i++];
	});
	return result;
}

myBuffer_t demap_impl::eqs_update(const myBuffer_t& in, int frame) {
	auto spilots = eqs_selSpilots(in, frame);

	assert(spilots.size() == config.scattered_pilots_count);
	// calculate cir
	volk_32fc_x2_multiply_32fc(reinterpret_cast<lv_32fc_t*>(eqs_inBufInverse),
			spilots.data(), eqs_scatteredPilotsInverse[frame].data(),
			config.scattered_pilots_count);

	// interpolate cir
	fftwf_execute(eqs_planInverse);

	std::transform(reinterpret_cast<myComplex_t*>(eqs_outBufInverse),
			reinterpret_cast<myComplex_t*>(eqs_outBufInverse)
					+ config.scattered_pilots_count,
			reinterpret_cast<myComplex_t*>(eqs_outBufInverse),
			[&](myComplex_t a) {
				return a / (float)config.scattered_pilots_count;
			});

	// zero input buffer
	std::fill(reinterpret_cast<myComplex_t*>(eqs_inBufForward),
			reinterpret_cast<myComplex_t*>(eqs_inBufForward) + config.carriers,
			0);

	int cc = config.scattered_pilots_count / 2;
	// align data
	std::copy(reinterpret_cast<myComplex_t*>(eqs_outBufInverse),
			reinterpret_cast<myComplex_t*>(eqs_outBufInverse) + cc,
			reinterpret_cast<myComplex_t*>(eqs_inBufForward));

	std::copy(reinterpret_cast<myComplex_t*>(eqs_outBufInverse) + cc,
			reinterpret_cast<myComplex_t*>(eqs_outBufInverse)
					+ config.scattered_pilots_count,
			reinterpret_cast<myComplex_t*>(eqs_inBufForward) + config.carriers
					- cc);

	// execute interpolation
	fftwf_execute(eqs_planForward);

	// copy usefull carriers
	auto tmp = myBuffer_t(config.carriers);
	std::copy(begin(in) + config.zeros_left, end(in) - config.zeros_right,
			begin(tmp));

	// apply cir
	auto result = myBuffer_t(config.carriers);

	assert(tmp.size() == result.size());

	std::transform(begin(tmp), end(tmp),
			reinterpret_cast<myComplex_t*>(eqs_outBufForward), begin(result),
			[&](myComplex_t a, myComplex_t b) {
				return a / b;
			});

	return result;
}

// ***************************************************************
// EQ SCATTERED END
// ***************************************************************

// ***************************************************************
// DS START
// ***************************************************************

myBuffer_t demap_impl::ds_update(const myBuffer_t& in, int frame) {
	assert(in.size() == config.carriers);
	auto result = myBuffer_t(config.data_carrier_count);

	auto it = begin(result);
	for (auto a : ds_dataIdx[frame]) {
		*it++ = in[a];
	}

	return result;
}

// ***************************************************************
// DS END
// ***************************************************************

// ***************************************************************
// DEMAP START
// ***************************************************************

int demap_impl::demap(const myComplex_t& complex, int depth) {
	myReal_t re = real(complex);
	myReal_t im = imag(complex);

	bool x = std::signbit(re);
	bool y = std::signbit(im);

	auto result = y << 1 | x;

	if (depth < DEMAP_DEPTH) {
		auto b = 1.f / (2 << depth);
		auto a = myComplex_t { std::abs(re) - DEMAP_X * b, std::abs(im)
				- DEMAP_Y * b };
		result = demap(a, depth + 1) << 2 | result;
	}

	return result;
}

myBitset_t demap_impl::demap_update(const myBuffer_t& complex) {
	assert(complex.size() == config.data_carrier_count);
	auto result = myBitset_t(bitCount);
	auto it { 0 };
	for (auto c : complex) {
		auto d = demap(c, 0);
		for (int i { 0 }; i < 6; i++) {
			result[it++] = d & 1;
			d = d >> 1;
		}
	}
	assert(it == config.data_carrier_count * 6);
	return result;
}

// ***************************************************************
// DEMAP END
// ***************************************************************

// ***************************************************************
// DEINT START
// ***************************************************************

myBitset_t demap_impl::deint_symbol(const myBitset_t& bitset, int frame) {
	auto tmp = myBitset_t(bitCount);
	if (frame % 2 == 0) {
		for (auto it { 0U }; it < config.H.size(); it++) {
			tmp[it] = bitset[config.H[it]];
		}
	} else {
		for (auto it { 0U }; it < config.H.size(); it++) {
			tmp[config.H[it]] = bitset[it];
		}
	}
	return tmp;
}

myBitset_t demap_impl::deint_bit(const myBitset_t& tmp) {
	auto result = myBitset_t(bitCount);
	for (auto outer { 0 }; outer < 48; outer++) {
		for (auto inner { 0 }; inner < 756; inner++) {
			result[config.bit_table[inner] - 1 + outer * 756] = tmp[inner
					+ outer * 756];
		}
	}
	return result;
}

myBitset_t demap_impl::deint_update(const myBitset_t& bitset, int frame) {
	auto tmp = deint_symbol(bitset, frame);
	return deint_bit(tmp);
}

// ***************************************************************
// DEINT END
// ***************************************************************

void demap_impl::forecast(int noutput_items,
		gr_vector_int &ninput_items_required) {
	ninput_items_required[0] = noutput_items;
	ninput_items_required[1] = noutput_items;
}

int demap_impl::general_work(int noutput_items, gr_vector_int &ninput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items) {
	const gr_complex *in0 = (const gr_complex *) input_items[0];
	const gr_complex *in1 = (const gr_complex *) input_items[1];
	char *out = (char *) output_items[0];
//	gr_complex *out1 = (gr_complex *) output_items[1];

	auto coeff = lv_cmake(std::sqrt(42.0f), 0.f);

	for (int i = 0; i < noutput_items; i++) {
		myBuffer_t d_eq(in0 + i * config.carriers,
				in0 + i * config.carriers + config.carriers);
		myBuffer_t d_fft(in1 + i * config.fft_len,
				in1 + i * config.fft_len + config.fft_len);
		auto _frame = frameNum(d_eq);

		auto _eqs = eqs_update(d_fft, _frame);
		auto _ds = ds_update(_eqs, _frame);
		auto _mul = myBuffer_t(config.data_carrier_count);

		volk_32fc_s32fc_multiply_32fc(_mul.data(), _ds.data(), coeff,
				config.data_carrier_count);

		auto _dem = demap_update(_mul);
#if 1
		auto _deint = deint_update(_dem, _frame);
//		std::copy_n(begin(_deint), bitCount, out + i*bitCount);
		char *outp = out + i * config.data_carrier_count;
		int count = 0;
		for (int j = 0; j < config.data_carrier_count; j++) {
			*outp = 0;
			for (int k = 5; k >= 0; k--) {
				*outp |= (_deint[count++] & 1) << k;
			}
			outp++;
		}
#else

		std::vector<char> _deint(bitCount);
		gpu_deinterleave(_dem.data(), _deint.data(), _frame);

		char *outp = out + i*config.data_carrier_count;
		int count = 0;
		for(int j = 0; j < config.data_carrier_count; j++) {
			*outp=0;
			for(int k = 5; k >= 0; k--) {
				*outp |= (_deint[count++] & 1)<< k;
			}
			outp++;
		}
#endif
//		std::copy_n(begin(_mul), config.data_carrier_count, out1 + i*config.data_carrier_count);
	}

	// Tell runtime system how many input items we consumed on
	// each input stream.
	consume_each(noutput_items);

	// Tell runtime system how many output items we produced.
	return noutput_items;
}

} /* namespace dvbt_rx */
} /* namespace gr */

