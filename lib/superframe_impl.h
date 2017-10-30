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

#ifndef INCLUDED_DVBT_RX_SUPERFRAME_IMPL_H
#define INCLUDED_DVBT_RX_SUPERFRAME_IMPL_H

#include <dvbt_rx/superframe.h>
#include <dvbt_rx/myConfig_t.h>

namespace gr {
namespace dvbt_rx {

class superframe_impl: public superframe {
private:
	static myConfig_t config;
	std::deque<bool> q0, q1, q2, q3, q4, q5, q6, q7, q8;
	myInteger_t synchronize(const myBufferB_t& out);
	int isInSync;
public:
	superframe_impl();
	~superframe_impl();

	// Where all the action really happens
	void forecast(int noutput_items, gr_vector_int &ninput_items_required);

	int general_work(int noutput_items, gr_vector_int &ninput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items);
};

} // namespace dvbt_rx
} // namespace gr

#endif /* INCLUDED_DVBT_RX_SUPERFRAME_IMPL_H */

