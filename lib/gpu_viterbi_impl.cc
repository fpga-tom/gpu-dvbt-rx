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

namespace gr {
  namespace dvbt_rx {

    gpu_viterbi::sptr
    gpu_viterbi::make()
    {
      return gnuradio::get_initial_sptr
        (new gpu_viterbi_impl());
    }

    /*
     * The private constructor
     */
    gpu_viterbi_impl::gpu_viterbi_impl()
      : gr::block("gpu_viterbi",
              gr::io_signature::make(1, 1, sizeof(char) * gpu_viterbi_size),
              gr::io_signature::make(1, 1, sizeof(char) * gpu_viterbi_size/2))
    {
    	gpu_viterbi_init();
    }

    /*
     * Our virtual destructor.
     */
    gpu_viterbi_impl::~gpu_viterbi_impl()
    {
    	gpu_viterbi_free();
    }

    void
    gpu_viterbi_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items;
    }

    int
    gpu_viterbi_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const char *in = (const char *) input_items[0];
      char *out = (char *) output_items[0];

      for(int i = 0; i < noutput_items; i++) {
    	  gpu_viterbi_decode(in + i*gpu_viterbi_size*2, out + i*gpu_viterbi_size);
      }

      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace dvbt_rx */
} /* namespace gr */

