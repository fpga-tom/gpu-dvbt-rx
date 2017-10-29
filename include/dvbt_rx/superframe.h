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


#ifndef INCLUDED_DVBT_RX_SUPERFRAME_H
#define INCLUDED_DVBT_RX_SUPERFRAME_H

#include <dvbt_rx/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace dvbt_rx {

    /*!
     * \brief <+description of block+>
     * \ingroup dvbt_rx
     *
     */
    class DVBT_RX_API superframe : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<superframe> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of dvbt_rx::superframe.
       *
       * To avoid accidental use of raw pointers, dvbt_rx::superframe's
       * constructor is in a private implementation
       * class. dvbt_rx::superframe::make is the public interface for
       * creating new instances.
       */
      static sptr make();
    };

  } // namespace dvbt_rx
} // namespace gr

#endif /* INCLUDED_DVBT_RX_SUPERFRAME_H */

