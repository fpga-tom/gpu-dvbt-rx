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
#include "gpu_viterbi_impl.h"
#include <array>

namespace gr {
namespace dvbt_rx {

gpu_viterbi::sptr gpu_viterbi::make() {
	return gnuradio::get_initial_sptr(new gpu_viterbi_impl());
}

/*
 * The private constructor
 */
gpu_viterbi_impl::gpu_viterbi_impl() :
		gr::block("gpu_viterbi",
				gr::io_signature::make(1, 1, sizeof(char) * config.data_carrier_count*Blocks),
				gr::io_signature::make(1, 1, sizeof(char) * config.data_carrier_count*Blocks/2)) {
	gpu_viterbi_init();
}

/*
 * Our virtual destructor.
 */
gpu_viterbi_impl::~gpu_viterbi_impl() {
	gpu_viterbi_free();
}

void gpu_viterbi_impl::forecast(int noutput_items,
		gr_vector_int &ninput_items_required) {
	ninput_items_required[0] = noutput_items;
}

int gpu_viterbi_impl::general_work(int noutput_items,
		gr_vector_int &ninput_items, gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items) {
	const char *in = (const char *) input_items[0];
	char *out = (char *) output_items[0];

	auto d_bsize = Blocks*12096;
	// Number of bits after depuncturing a block (before decoding)
	auto d_m = 6;
	auto d_nbits = 2 * d_k * d_bsize;
	char d_inbits[Blocks*12096*4];
	int nblocks = 8 * noi / (d_bsize * d_k);
	auto d_nsymbols = d_bsize * d_n / d_m;


	for (int k = 0; k < noutput_items; k++) {
		const char *inp = in + k * config.data_carrier_count*Blocks;

		memset(d_inbits, 0, d_nbits);
		for (int n = 0; n < 1; n++) {
			for (int count = 0, i = 0, bc = 0; i < d_nsymbols; i++) {
				for (int j = (d_m-1) ; j >= 0; j--) {
					int byte = count >> 1;
					int bit = count & 1;
					// Depuncture
					while (d_puncture[count % (2 * d_k)] == 0) {
						d_inbits[byte] |= bit ? 2 << 1 : 2 << 2;
						count++;
						byte = count >> 1;
						bit = count & 1;
					}

					// Insert received bits
					d_inbits[byte] |= ((inp[(n * d_nsymbols) + i] >> j) & 1) << ( bit ? 0 : 1);
					count++;

					// Depuncture
//					while (d_puncture[count % (2 * d_k)] == 0)
//						d_inbits[count++] = 2;
				}
			}
			gpu_viterbi_decode(d_inbits, out + k * noi);
		}
	}


	// Tell runtime system how many input items we consumed on
	// each input stream.
	consume_each(noutput_items);

	// Tell runtime system how many output items we produced.
	return noutput_items;
}

} /* namespace dvbt_rx */
} /* namespace gr */

