/*
 * Copyright 2012 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * This class gathers together all the test cases for the gr-filter
 * directory into a single test suite.  As you create new test cases,
 * add them here.
 */

#include "qa_dvbt_rx.h"
#include "qa_sync_cc.h"
#include "qa_demap.h"
#include "qa_gpu_viterbi.h"

CppUnit::TestSuite *
qa_dvbt_rx::suite()
{
  CppUnit::TestSuite *s = new CppUnit::TestSuite("dvbt_rx");
  s->addTest(gr::dvbt_rx::qa_sync_cc::suite());
  s->addTest(gr::dvbt_rx::qa_demap::suite());
  s->addTest(gr::dvbt_rx::qa_gpu_viterbi::suite());

  return s;
}
