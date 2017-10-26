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

#ifndef INCLUDED_DVBT_RX_MYCONFIG_T_H
#define INCLUDED_DVBT_RX_MYCONFIG_T_H

#include <dvbt_rx/api.h>
#include <bitset>
#include <deque>
#include <vector>

namespace gr {
namespace dvbt_rx {

const int bitCount = 36288;

typedef float myReal_t;
typedef int myInteger_t;
typedef gr_complex myComplex_t;
typedef std::vector<myComplex_t> myBuffer_t;
typedef std::vector<myReal_t> myBufferR_t;
typedef std::vector<unsigned char> myBufferB_t;
typedef std::vector<myInteger_t> myBufferI_t;
typedef std::deque<myComplex_t> myDelay_t;
typedef std::vector<bool> myBitset_t;

/*!
 * \brief <+description+>
 *
 */
class DVBT_RX_API myConfig_t {
public:
	myConfig_t();
	~myConfig_t();

	static myInteger_t fft_len;
	static myInteger_t cp_len;
	static myInteger_t sym_len;
	static myInteger_t zeros_left;
	static myInteger_t zeros_right;
	static myInteger_t carriers;
	static myInteger_t carrier_center;
	static myInteger_t data_carrier_count;
	static myReal_t sample_rate;
	static std::vector<myInteger_t> continual_pilots;
	static size_t continual_pilots_count;
	static std::vector<myReal_t> continual_pilots_value;
	static std::vector<myInteger_t> tps;
	static std::vector<std::vector<myInteger_t> > scattered_pilots;
	static std::vector<std::vector<myReal_t> > scattered_pilots_value;
	static size_t scattered_pilots_count;
	static std::vector<myInteger_t> H;
	static std::vector<myInteger_t> bit_table;
private:
};

} // namespace dvbt_rx
} // namespace gr

#endif /* INCLUDED_DVBT_RX_MYCONFIG_T_H */

