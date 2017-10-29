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
#include "descrambler_impl.h"
#include <sys/types.h>
#include <sys/wait.h>


namespace gr {
  namespace dvbt_rx {

    descrambler::sptr
    descrambler::make()
    {
      return gnuradio::get_initial_sptr
        (new descrambler_impl());
    }

    /*
     * The private constructor
     */
    descrambler_impl::descrambler_impl()
      : gr::block("descrambler",
              gr::io_signature::make(1, 1, sizeof(unsigned char) * 1632),
              gr::io_signature::make(1, 1, sizeof(unsigned char) * 1504)), frame(0)
    {
    	pipe(parentRead);
    	pipe(parentWrite);
    	pid_t childpid = fork();
    	if (childpid == -1) {
    		perror("fork");
    		exit(1);
    	}

    	if (childpid == 0) {
    		close(0);
    		dup(parentWrite[0]);
    		close(1);
    		dup(parentRead[1]);
    		execlp("/root/reedsolomon",
    				"/root/reedsolomon", nullptr);
    		perror("execlp");
    		exit(1);
    	} else {
    		close(parentRead[1]);
    		close(parentWrite[0]);
    	}

    }

    /*
     * Our virtual destructor.
     */
    descrambler_impl::~descrambler_impl()
    {
    	close(parentWrite[1]);
    	close(parentRead[0]);
    	int status;
    	wait(&status);
    	std::cout << "reedsolomon exited: " << status << std::endl;

    }

    myBufferB_t descrambler_impl::descrambler_update(const myBufferB_t& buf) {
    	assert(buf.size() == 1632);
    	if (write(parentWrite[1], buf.data(), buf.size()) != buf.size()) {
    		perror("write");
    	}


    	auto result = myBufferB_t(1504);
    	auto count = 0;
    	do {
    		auto r = read(parentRead[0], result.data() + count,
    				result.size() - count);
    		if (r <= 0) {
    			perror("deinterleaver read");
    		}
    		count += r;
    	} while (count != result.size());

    	return result;

    }


    void
    descrambler_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items;
    }

    int
    descrambler_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned char *in = (const unsigned char *) input_items[0];
      unsigned char *out = (unsigned char *) output_items[0];

      for(int i = 0; i < noutput_items; i++) {
    	  const unsigned char* start = in + i*1632;
    	  myBufferB_t d_in(start, start + 1632);

    	  myBufferB_t result = descrambler_update(d_in);
    	  unsigned char *outp = out + i* 1504;
    	  std::copy(begin(result), end(result), outp);
    	  frame++;
      }

      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each (noutput_items);

      // Tell runtime system how many output items we produced.
      if(frame > 20)
    	  return noutput_items;
      else
    	  return 0;
    }

  } /* namespace dvbt_rx */
} /* namespace gr */

