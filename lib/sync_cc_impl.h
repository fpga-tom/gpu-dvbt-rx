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

#ifndef INCLUDED_DVBT_RX_SYNC_CC_IMPL_H
#define INCLUDED_DVBT_RX_SYNC_CC_IMPL_H

#include <fftw3.h>
#include <dvbt_rx/sync_cc.h>
#include <dvbt_rx/myConfig_t.h>

namespace gr {
namespace dvbt_rx {

const myReal_t PI2 = 2 * M_PI;
const myReal_t NCO_P_GAIN = 1e-8;
const myReal_t NCO_I_GAIN = 5e-2;

const myReal_t SYNC_P_GAIN = 1e-10;
const myReal_t SYNC_I_GAIN = 5e-4;

const myInteger_t lockCount = 5;

const myReal_t SRO_N = 10;
const myReal_t SRO_P_GAIN = 1e-5;
const myReal_t SRO_I_GAIN = 4e-3;
const myReal_t SRO_P_GAIN_RFO = 1e-5;
const myReal_t SRO_I_GAIN_RFO = 1e-2;




class sync_cc_impl: public sync_cc {
private:
	const myConfig_t config;

	// ***************************************************************
	// NCO START
	// ***************************************************************

	lv_32fc_t nco_phase;

	// pi controller integral part
	myReal_t nco_integral;

		// helper methods
	myReal_t nco_correction(myReal_t&, myReal_t&, myReal_t&);
	myBuffer_t nco_freqShift(myBuffer_t&, myReal_t&);

	myBuffer_t nco_update(myBuffer_t& in, myReal_t ifo, myReal_t f, myReal_t r);

	// ***************************************************************
	// NCO END
	// ***************************************************************

	// ***************************************************************
	// SYNC START
	// ***************************************************************
	myBuffer_t delay; 							// correlator delay reg
	myDelay_t accDelay;  				// correlator accumulator helper delay
	myComplex_t acc; 							// corelator accumulator
	unsigned long current_size;

	// peak finder variables
	std::deque<myInteger_t> peakDelay;
	int accPeak;
	myReal_t integral;
	myReal_t peak;

	int currentLock;

	// framer variables
	std::shared_ptr<myBuffer_t> current;		// current frame
	std::shared_ptr<myBuffer_t> next;			// next frame

	myBuffer_t
	sync_correlate(const myBuffer_t& in, myBuffer_t& delay,
			myDelay_t& accDelay, myComplex_t& acc);
	size_t sync_findPeak(const myBuffer_t& b);
	myBuffer_t sync_align(const myBuffer_t& in, myInteger_t peak);
	myBuffer_t sync_update(const myBuffer_t& in, const myReal_t fineTiming,
			myReal_t& _freq, bool& _locked);

	// ***************************************************************
	// SYNC END
	// ***************************************************************

	// ***************************************************************
	// FFT START
	// ***************************************************************

	fftwf_complex *fft_inBuf, *fft_outBuf;
	fftwf_plan_s* fft_plan;

	myBuffer_t fft_update(const myBuffer_t& in);

	// ***************************************************************
	// FFT END
	// ***************************************************************

	// ***************************************************************
	// RFO START
	// ***************************************************************

	myReal_t integral_rfo;
	myBuffer_t prevrfo;

	myBuffer_t cpilots(const myBuffer_t& complex);
	myReal_t rfo(const myBuffer_t& b);

	// ***************************************************************
	// RFO END
	// ***************************************************************

	// ***************************************************************
	// IFO START
	// ***************************************************************

	myBuffer_t ifo_prev;

	int ifo_update(myBuffer_t& in);

	// ***************************************************************
	// IFO END
	// ***************************************************************

	// ***************************************************************
	// EQ CONTINUAL START
	// ***************************************************************

	// inverse fft variables
	fftwf_complex *eq_inBufInverse, *eq_outBufInverse;
	fftwf_plan_s *eq_planInverse;

		// forward fft variables
	fftwf_complex *eq_inBufForward, *eq_outBufForward;
	fftwf_plan_s *eq_planForward;

		// helper methods
	myBuffer_t selCpilots(const myBuffer_t&);


	std::tuple<myBuffer_t, myBuffer_t> eq_update(const myBuffer_t& in);

	// ***************************************************************
	// EQ CONTINUAL END
	// ***************************************************************

	// ***************************************************************
	// FTO START
	// ***************************************************************

	myBuffer_t fto_x;

	myReal_t fto_update(const myBuffer_t& in);

	// ***************************************************************
	// FTO END
	// ***************************************************************

	myInteger_t _ifo;
	myReal_t _f;
	myReal_t _rfo;
	myReal_t _fto;


public:
	sync_cc_impl();
	~sync_cc_impl();

	// Where all the action really happens
	void forecast(int noutput_items, gr_vector_int &ninput_items_required);

	int general_work(int noutput_items, gr_vector_int &ninput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items);
};

} // namespace dvbt_rx
} // namespace gr

#endif /* INCLUDED_DVBT_RX_SYNC_CC_IMPL_H */

