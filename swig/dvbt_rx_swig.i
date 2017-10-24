/* -*- c++ -*- */

#define DVBT_RX_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "dvbt_rx_swig_doc.i"

%{
#include "dvbt_rx/sync_cc.h"
#include "dvbt_rx/myConfig_t.h"
%}


%include "dvbt_rx/sync_cc.h"
GR_SWIG_BLOCK_MAGIC2(dvbt_rx, sync_cc);
%include "dvbt_rx/myConfig_t.h"
