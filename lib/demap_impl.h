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

#ifndef INCLUDED_DVBT_RX_DEMAP_IMPL_H
#define INCLUDED_DVBT_RX_DEMAP_IMPL_H

#include <dvbt_rx/demap.h>
#include <dvbt_rx/myConfig_t.h>
#include <fftw3.h>
#include <volk/volk.h>

void gpu_deint_init();
void gpu_deint_free();
void gpu_deinterleave(const char* data, char* output, int frame);

namespace gr {
  namespace dvbt_rx {

    class demap_impl : public demap
    {
     private:
    	myConfig_t config;

    	// ***************************************************************
    	// DEMAP START
    	// ***************************************************************

    	int frameNum(const myBuffer_t& in);

    	// ***************************************************************
    	// DEMAP END
    	// ***************************************************************

    	// ***************************************************************
    	// EQ SCATTERED START
    	// ***************************************************************

    	std::vector<myBuffer_t> eqs_spilotsBuf;
		std::vector<int> eqs_scattered_indices;
		// inverse fft variables
		fftwf_complex *eqs_inBufInverse, *eqs_outBufInverse;
		fftwf_plan_s *eqs_planInverse;

		// forward fft variables
		fftwf_complex *eqs_inBufForward, *eqs_outBufForward;
		fftwf_plan_s *eqs_planForward;

		std::vector<myBuffer_t> eqs_scatteredPilotsInverse;

		// helper methods
		myBuffer_t eqs_selSpilots(const myBuffer_t&, int frame);

    	myBuffer_t eqs_update(const myBuffer_t& in, int frame);

        // ***************************************************************
        // EQ SCATTERED END
        // ***************************************************************


    	// ***************************************************************
    	// DS START
    	// ***************************************************************

    	std::vector<std::vector<int>> ds_dataIdx;

    	myBuffer_t ds_update(const myBuffer_t& in, int frame);

    	// ***************************************************************
    	// DS END
    	// ***************************************************************


    	// ***************************************************************
    	// DEMAP START
    	// ***************************************************************

    	const float DEMAP_X = 8;
    	const float DEMAP_Y = 8;
    	const int DEMAP_DEPTH = 2;

    	int demap(const myComplex_t& complex, int depth);

    	myBitset_t demap_update(const myBuffer_t& complex);


    	// ***************************************************************
    	// DEMAP END
    	// ***************************************************************

    	// ***************************************************************
    	// DEINT START
    	// ***************************************************************

    	myBitset_t deint_symbol(const myBitset_t& bitset, int frame);
    	myBitset_t deint_bit(const myBitset_t& tmp);
    	myBitset_t deint_update(const myBitset_t& bitset, int frame);

    	// ***************************************************************
    	// DEINT END
    	// ***************************************************************

     public:
      demap_impl();
      ~demap_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace dvbt_rx
} // namespace gr

#endif /* INCLUDED_DVBT_RX_DEMAP_IMPL_H */

