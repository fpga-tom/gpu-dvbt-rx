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
#include "superframe_impl.h"

namespace gr {
namespace dvbt_rx {

superframe::sptr superframe::make() {
	return gnuradio::get_initial_sptr(new superframe_impl());
}

/*
 * The private constructor
 */
superframe_impl::superframe_impl() :
		gr::block("superframe", gr::io_signature::make(1, 1, sizeof(char)),
				gr::io_signature::make(1, 1, sizeof(char))), isInSync(-1), restartSync(false) {
	q0 = std::deque<bool>(204 * 8);
	q1 = std::deque<bool>(204 * 7);
	q2 = std::deque<bool>(204 * 6);
	q3 = std::deque<bool>(204 * 5);
	q4 = std::deque<bool>(204 * 4);
	q5 = std::deque<bool>(204 * 3);
	q6 = std::deque<bool>(204 * 2);
	q7 = std::deque<bool>(204 * 1);

	 message_port_register_in(pmt::mp("restartSync"));
	 set_msg_handler(pmt::mp("restartSync"), boost::bind(&superframe_impl::restartSyncHandler, this, _1));
}

/*
 * Our virtual destructor.
 */
superframe_impl::~superframe_impl() {
}

void superframe_impl::restartSyncHandler(pmt::pmt_t msg) {
	restartSync = true;
	std::cout << "restartSync" << std::endl;
}

myInteger_t superframe_impl::synchronize(const myBufferB_t& out) {

	auto inSync = false;
	auto syncCounter = -1;

	if (!inSync) {
		for (auto it { 0 }; it < out.size(); it++) {
			auto a = out[it];
			if (!inSync) {
				inSync = q0.front() && q1.front() && q2.front() && q3.front()
						&& q4.front() && q5.front() && q6.front() && q7.front();
				if(inSync)
					syncCounter = it;
			}

			q0.pop_front();
			q1.pop_front();
			q2.pop_front();
			q3.pop_front();
			q4.pop_front();
			q5.pop_front();
			q6.pop_front();
			q7.pop_front();

			q0.push_back((bool) (a == 0xb8));
			q1.push_back((bool) (a == 0x47));
			q2.push_back((bool) (a == 0x47));
			q3.push_back((bool) (a == 0x47));
			q4.push_back((bool) (a == 0x47));
			q5.push_back((bool) (a == 0x47));
			q6.push_back((bool) (a == 0x47));
			q7.push_back((bool) (a == 0x47));

		}
	}
	return syncCounter;
}

void superframe_impl::forecast(int noutput_items,
		gr_vector_int &ninput_items_required) {
	ninput_items_required[0] = noutput_items;
}

int superframe_impl::general_work(int noutput_items,
		gr_vector_int &ninput_items, gr_vector_const_void_star &input_items,
		gr_vector_void_star &output_items) {
	const char *in = (const char *) input_items[0];
	char *out = (char *) output_items[0];
	int oi = 0;

	myBufferB_t d_in(in, in + noutput_items);

	if (isInSync < 0) {
		isInSync = synchronize(d_in);
	}
	if (isInSync >= 0) {
		restartSync = false;
		oi = noutput_items - isInSync;
		std::copy(begin(d_in) + isInSync, end(d_in), out);
		isInSync = 0;
	}

	// Tell runtime system how many input items we consumed on
	// each input stream.
	consume_each(noutput_items);

	// Tell runtime system how many output items we produced.
	return oi;
}

} /* namespace dvbt_rx */
} /* namespace gr */

