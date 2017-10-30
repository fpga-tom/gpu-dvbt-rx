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
#include <volk/volk.h>
#include "sync_cc_impl.h"

namespace gr {
namespace dvbt_rx {

sync_cc::sptr sync_cc::make() {
	return gnuradio::get_initial_sptr(new sync_cc_impl());
}

/*
 * The private constructor
 */
sync_cc_impl::sync_cc_impl() :
		gr::block("sync_cc",
				gr::io_signature::make(1, 1,
						sizeof(gr_complex) * config.sym_len),
				gr::io_signature::make2(2, 2,
						sizeof(gr_complex) * config.fft_len,
						sizeof(gr_complex) * config.carriers)), delay(
				config.fft_len + config.sym_len), accDelay(config.cp_len), acc {
				0 }, current_size { 0 }, peakDelay(lockCount), accPeak { 0 }, integral {
				0 }, peak { 0 }, currentLock { 0 }, prevrfo(
				config.continual_pilots_count), ifo_prev(config.fft_len), fto_x(
				config.continual_pilots_count - 1), _fto { 0. }, _ifo { 0 }, _rfo {
				0 }, nco_phase { lv_cmake(1.0f, 0.0f) }, nco_integral { 0 } {

	current = std::make_shared < myBuffer_t > (config.sym_len);
	next = std::make_shared < myBuffer_t > (config.sym_len);

	fft_inBuf = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.fft_len));

	fft_outBuf = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.fft_len));

	fft_plan = fftwf_plan_dft_1d(config.fft_len, fft_inBuf, fft_outBuf,
			FFTW_FORWARD,
			FFTW_ESTIMATE);

	eq_inBufInverse = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.continual_pilots_count));
	eq_outBufInverse = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.continual_pilots_count));

	eq_planInverse = fftwf_plan_dft_1d(config.continual_pilots_count,
			eq_inBufInverse, eq_outBufInverse, FFTW_BACKWARD, FFTW_ESTIMATE);

	eq_inBufForward = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.carriers));
	eq_outBufForward = reinterpret_cast<fftwf_complex*>(fftwf_malloc(
			sizeof(fftwf_complex) * config.carriers));

	eq_planForward = fftwf_plan_dft_1d(config.carriers, eq_inBufForward,
			eq_outBufForward, FFTW_FORWARD, FFTW_ESTIMATE);

	std::transform(begin(config.continual_pilots_value),
			end(config.continual_pilots_value) - 1,
			begin(config.continual_pilots_value) + 1, begin(fto_x),
			[](myComplex_t a, myComplex_t b) {
				return std::conj(a) * b;
			});
}

/*
 * Our virtual destructor.
 */
sync_cc_impl::~sync_cc_impl() {
	fftwf_free(fft_inBuf);
	fftwf_free(fft_outBuf);
	fftwf_destroy_plan(fft_plan);

	fftwf_free(eq_inBufInverse);
	fftwf_free(eq_outBufInverse);
	fftwf_destroy_plan(eq_planInverse);
	fftwf_free(eq_inBufForward);
	fftwf_free(eq_outBufForward);
	fftwf_destroy_plan(eq_planForward);
}

//****************************************************************************
// NCO START
//****************************************************************************

myReal_t sync_cc_impl::nco_correction(myReal_t& ifo, myReal_t& f, myReal_t& r) {
	myReal_t fcorr { 0.f };
	if (std::abs(ifo) >= 1.0f) {
		fcorr = -ifo * config.sample_rate / config.fft_len;
	} else {
		if (std::abs(f) >= 0.5f) {
			fcorr = -f;
		} else {
			fcorr = -r;
		}
	}
	auto proportional = NCO_P_GAIN * fcorr;
	nco_integral += NCO_I_GAIN * fcorr;
	return proportional + nco_integral;
}

myBuffer_t sync_cc_impl::nco_freqShift(myBuffer_t& in, myReal_t& corr) {
	assert(in.size() == config.sym_len);
	auto result = myBuffer_t(config.sym_len);

	auto frequency = PI2 * corr / config.sample_rate;
	lv_32fc_t phase_increment = lv_cmake(std::cos(frequency),
			std::sin(frequency));
	volk_32fc_s32fc_x2_rotator_32fc(result.data(), in.data(), phase_increment,
			&nco_phase, config.sym_len);

	return result;
}

myBuffer_t sync_cc_impl::nco_update(myBuffer_t& in, myReal_t ifo, myReal_t f,
		myReal_t r) {
	auto c = nco_correction(ifo, f, r);
	return nco_freqShift(in, c);
}

//****************************************************************************
// NCO END
//****************************************************************************

//****************************************************************************
// SYNC START
//****************************************************************************

myBuffer_t sync_cc_impl::sync_correlate(const myBuffer_t& in, myBuffer_t& delay,
		myDelay_t& accDelay, myComplex_t& acc) {

	myBuffer_t b(config.sym_len);

	std::copy_n(begin(in), config.sym_len, begin(delay) + config.fft_len);

	volk_32fc_x2_multiply_conjugate_32fc(b.data(), in.data(), delay.data(),
			config.sym_len);

	std::copy(begin(delay) + config.sym_len, end(delay), begin(delay));

	std::transform(begin(b), end(b), begin(b), [&](myComplex_t prod) {
		acc = acc + prod;
		acc = acc - accDelay.front();
		accDelay.pop_front();
		accDelay.push_back(prod);

		return acc;
	});

	return b;
}

size_t sync_cc_impl::sync_findPeak(const myBuffer_t& b) {
	auto max =
			std::max_element(begin(b), end(b),
					[](myComplex_t a, myComplex_t b) {return std::abs(a) < std::abs(b);});

	auto peakIdx = std::distance(begin(b), max);

	auto last = peakDelay.front();
	accPeak = accPeak + peakIdx - last;
	peakDelay.pop_front();
	peakDelay.push_back(peakIdx);
	auto result = accPeak / (myReal_t) lockCount;
	assert(result >= 0);
	assert(result < config.sym_len);
	return result;
}

myBuffer_t sync_cc_impl::sync_align(const myBuffer_t& in, myInteger_t peak) {
	assert(peak >= 0);
	assert(peak < config.sym_len);
	auto n = config.sym_len - current_size;
	std::copy_n(begin(in), n, begin(*current) + current_size);
	current_size = config.sym_len - peak;
	std::copy_n(begin(in) + peak, current_size, begin(*next));
	assert(current->size() == config.sym_len);
	std::swap(next, current);
	return *next;

}

myBuffer_t sync_cc_impl::sync_update(const myBuffer_t& in,
		const myReal_t fineTiming, myReal_t& _freq, bool& _locked) {

	auto b = sync_correlate(in, delay, accDelay, acc);
	auto peakFind = sync_findPeak(b);
	if (currentLock < lockCount + 5) {
		peak = peakFind;
		integral = peak;
		currentLock++;
	} else {
		auto ft = -fineTiming;
		auto proportional = SYNC_P_GAIN * ft;
		integral += SYNC_I_GAIN * ft;
		peak = proportional + integral;
	}

	while (peak >= config.sym_len) {
		peak -= config.sym_len;
	}
	while (peak < 0) {
		peak += config.sym_len;
	}

	bool locked = !(currentLock >= lockCount + 5
			&& std::abs(peak - peakFind) > 2000);
	if (!locked) {
		std::cerr << "Lost sync " << fineTiming << " " << peak << " "
				<< peakFind << std::endl;
		// restart sync
		currentLock = 0;
	}
	auto freq = std::arg(b[std::floor(peak)]) / 2.0 / M_PI / config.fft_len
			* config.sample_rate;
	auto result = sync_align(in, std::floor(peak));
	_freq = freq;
	_locked = locked;
	return result;
}

//****************************************************************************
// SYNC END
//****************************************************************************

//****************************************************************************
// FFT START
//****************************************************************************

myBuffer_t sync_cc_impl::fft_update(const myBuffer_t& in) {
	auto out = myBuffer_t(config.fft_len);
	auto tmp = myBuffer_t(config.fft_len);

	// to avoid ISI, we sample at half of cyclic prefix
	std::copy(begin(in) + config.cp_len / 2, end(in) - config.cp_len / 2,
			begin(tmp));

	std::copy(begin(tmp), begin(tmp) + config.cp_len / 2,
			reinterpret_cast<myComplex_t*>(fft_inBuf) + config.fft_len
					- config.cp_len / 2);
	std::copy(begin(tmp) + config.cp_len / 2, end(tmp),
			reinterpret_cast<myComplex_t*>(fft_inBuf));

	fftwf_execute(fft_plan);

	std::copy(reinterpret_cast<myComplex_t*>(fft_outBuf),
			reinterpret_cast<myComplex_t*>(fft_outBuf) + config.fft_len / 2,
			begin(out) + config.fft_len / 2);
	std::copy(reinterpret_cast<myComplex_t*>(fft_outBuf) + config.fft_len / 2,
			reinterpret_cast<myComplex_t*>(fft_outBuf) + config.fft_len,
			begin(out));

	return out;
}

//****************************************************************************
// FFT END
//****************************************************************************

//****************************************************************************
// RFO START
//****************************************************************************

myBuffer_t sync_cc_impl::cpilots(const myBuffer_t& complex) {
	assert(complex.size() == config.fft_len);
	auto result = myBuffer_t(config.continual_pilots_count);
	std::transform(begin(config.continual_pilots), end(config.continual_pilots),
			begin(result), [&](myInteger_t a) {
				return complex[a + config.zeros_left];
			});
	return result;
}

myReal_t sync_cc_impl::rfo(const myBuffer_t& b) {
	assert(b.size() == config.fft_len);
	auto complex = cpilots(b);
	assert(complex.size() == config.continual_pilots_count);
	auto tmp = myBufferR_t(config.continual_pilots_count);
	std::transform(begin(complex), end(complex), begin(prevrfo), begin(tmp),
			[&](myComplex_t a, myComplex_t b) {
				return std::arg(a*std::conj(b));
			});
	auto result = std::accumulate(begin(tmp), end(tmp), myReal_t { 0.f })
			/ (2 * M_PI * (config.sym_len) / config.fft_len)
			/ config.continual_pilots_count;

	std::copy(begin(complex), end(complex), begin(prevrfo));
	auto proportional = result * SRO_P_GAIN_RFO;
	integral_rfo += result * SRO_I_GAIN_RFO;
//	return proportional + integral_rfo;
	return result;
}

//****************************************************************************
// RFO END
//****************************************************************************

// ***************************************************************
// IFO START
// ***************************************************************

int sync_cc_impl::ifo_update(myBuffer_t& in) {

	const int low = -4;
	const int hi = 4;

	std::vector<myReal_t> maxs(hi - low + 1);

	for (auto i { low }; i <= hi; i++) {

		auto base { config.zeros_left + i };
		auto tmp1 = myBuffer_t(config.continual_pilots_count);
		auto tmp2 = myBuffer_t(config.continual_pilots_count);

		std::transform(begin(config.continual_pilots),
				end(config.continual_pilots), begin(tmp1), [&](myInteger_t t) {
					return in[base + t];
				});

		std::transform(begin(config.continual_pilots),
				end(config.continual_pilots), begin(tmp2), [&](myInteger_t t) {
					return ifo_prev[base + t];
				});

		auto acc = myComplex_t { 0.f, 0.f };
		volk_32fc_x2_conjugate_dot_prod_32fc(&acc, tmp1.data(), tmp2.data(),
				config.continual_pilots_count);

		maxs[i + hi] = std::abs(acc);
	}

	std::copy(begin(in), end(in), begin(ifo_prev));

	auto max = std::max_element(begin(maxs), end(maxs));
	return std::distance(begin(maxs), max) - hi;

}

// ***************************************************************
// IFO END
// ***************************************************************

// ***************************************************************
// EQ CONTINUAL START
// ***************************************************************

myBuffer_t sync_cc_impl::selCpilots(const myBuffer_t& in) {
	assert(in.size() == config.fft_len);

	auto result = myBuffer_t(config.continual_pilots.size());
	auto i = begin(config.continual_pilots);
	std::generate(begin(result), end(result), [&]() {
		return in[config.zeros_left + *i++];
	});
	return result;
}

std::tuple<myBuffer_t, myBuffer_t> sync_cc_impl::eq_update(
		const myBuffer_t& in) {

	auto cpilots = selCpilots(in);

	assert(cpilots.size() == config.continual_pilots_count);
	// calculate cir
	auto c = begin(config.continual_pilots_value);
	std::transform(begin(cpilots), end(cpilots),
			reinterpret_cast<myComplex_t*>(eq_inBufInverse),
			[&](myComplex_t v) {
				return v * std::conj(*c++);
			});

	// interpolate cir
	fftwf_execute(eq_planInverse);

	std::transform(reinterpret_cast<myComplex_t*>(eq_outBufInverse),
			reinterpret_cast<myComplex_t*>(eq_outBufInverse)
					+ config.continual_pilots_count,
			reinterpret_cast<myComplex_t*>(eq_outBufInverse),
			[&](myComplex_t a) {
				return a / (float)config.continual_pilots_count;
			});

	// zero input buffer
	std::fill(reinterpret_cast<myComplex_t*>(eq_inBufForward),
			reinterpret_cast<myComplex_t*>(eq_inBufForward) + config.carriers,
			0);

	// align data
	std::copy(reinterpret_cast<myComplex_t*>(eq_outBufInverse),
			reinterpret_cast<myComplex_t*>(eq_outBufInverse)
					+ config.carrier_center,
			reinterpret_cast<myComplex_t*>(eq_inBufForward));

	std::copy(
			reinterpret_cast<myComplex_t*>(eq_outBufInverse)
					+ config.carrier_center + 1,
			reinterpret_cast<myComplex_t*>(eq_outBufInverse)
					+ config.continual_pilots_count,
			reinterpret_cast<myComplex_t*>(eq_inBufForward) + config.carriers
					- config.carrier_center);

	// execute interpolation
	fftwf_execute(eq_planForward);

	// copy usefull carriers
	auto tmp = myBuffer_t(config.carriers);
	std::copy(begin(in) + config.zeros_left, end(in) - config.zeros_right,
			begin(tmp));

	// apply cir
	auto result = myBuffer_t(config.carriers);

	assert(tmp.size() == result.size());

	std::transform(begin(tmp), end(tmp),
			reinterpret_cast<myComplex_t*>(eq_outBufForward), begin(result),
			[&](myComplex_t a, myComplex_t b) {
				return a / b;
			});

	std::tuple<myBuffer_t, myBuffer_t> retval(result, cpilots);
	return retval;

}

// ***************************************************************
// EQ CONTINUAL END
// ***************************************************************

// ***************************************************************
// FTO START
// ***************************************************************

myReal_t sync_cc_impl::fto_update(const myBuffer_t& in) {
	assert(in.size() == config.continual_pilots_count);
	auto z = myBuffer_t(config.continual_pilots_count - 1);
	auto y = myBuffer_t(config.continual_pilots_count - 1);

	std::copy(begin(in) + 1, end(in), begin(y));

	volk_32fc_x2_multiply_conjugate_32fc(z.data(), y.data(), in.data(),
			config.continual_pilots_count - 1);

	auto acc = myComplex_t { 0.f, 0.f };
	volk_32fc_x2_conjugate_dot_prod_32fc(&acc, z.data(), fto_x.data(),
			config.continual_pilots_count - 1);

	auto result = (config.fft_len) * std::arg(acc) / 2.0 / M_PI;
	return result;
}

// ***************************************************************
// FTO END
// ***************************************************************

void sync_cc_impl::forecast(int noutput_items,
		gr_vector_int &ninput_items_required) {
	ninput_items_required[0] = noutput_items;
}

int sync_cc_impl::general_work(int noutput_items, gr_vector_int &ninput_items,
		gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items) {
	const gr_complex *in = (const gr_complex *) input_items[0];
	gr_complex *out = (gr_complex *) output_items[0];
	gr_complex *eq_out = (gr_complex *) output_items[1];

	bool locked;

	for (int i = 0; i < noutput_items; i++) {

		myBuffer_t d_in(config.sym_len);
		std::copy_n(in + i * config.sym_len, config.sym_len, begin(d_in));

		auto _nco = nco_update(d_in, _ifo, _f, _rfo);
		auto _sync = sync_update(_nco, _fto, _f, locked);
		auto _fft = fft_update(_sync);
		_rfo = rfo(_fft);
		_ifo = ifo_update(_fft);
		std::tuple<myBuffer_t, myBuffer_t> _eq = eq_update(_fft);
		_fto = fto_update(std::get < 1 > (_eq));
		std::copy(begin(_fft), end(_fft), out + i * config.fft_len);
		std::copy(begin(std::get < 0 > (_eq)), end(std::get < 0 > (_eq)),
				eq_out + i * config.carriers);
	}

	// Tell runtime system how many input items we consumed on
	// each input stream.
	consume_each(noutput_items);

	// Tell runtime system how many output items we produced.
	return noutput_items;
}

} /* namespace dvbt_rx */
} /* namespace gr */

