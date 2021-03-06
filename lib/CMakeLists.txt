# Copyright 2011,2012 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

list(APPEND dvbt_rx_sources
    sync_cc_impl.cc
    myConfig_t.cc
    demap_impl.cc
    gpu_viterbi_impl.cc
    descrambler_impl.cc
    superframe_impl.cc
)

set(dvbt_rx_sources "${dvbt_rx_sources}" PARENT_SCOPE)
if(NOT dvbt_rx_sources)
	MESSAGE(STATUS "No C++ sources... skipping lib/")
	return()
endif(NOT dvbt_rx_sources)

add_library(gnuradio-dvbt_rx SHARED ${dvbt_rx_sources})
target_link_libraries(gnuradio-dvbt_rx ${Boost_LIBRARIES} ${GNURADIO_ALL_LIBRARIES} fftw3f viterbi_002)
set_target_properties(gnuradio-dvbt_rx PROPERTIES DEFINE_SYMBOL "gnuradio_dvbt_rx_EXPORTS")

if(APPLE)
    set_target_properties(gnuradio-dvbt_rx PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
install(TARGETS gnuradio-dvbt_rx
    LIBRARY DESTINATION lib${LIB_SUFFIX} # .so/.dylib file
    ARCHIVE DESTINATION lib${LIB_SUFFIX} # .lib file
    RUNTIME DESTINATION bin              # .dll file
)

########################################################################
# Build and register unit test
########################################################################
include(GrTest)

include_directories(${CPPUNIT_INCLUDE_DIRS})

list(APPEND test_dvbt_rx_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_dvbt_rx.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_dvbt_rx.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_superframe.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_descrambler.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_gpu_viterbi.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_demap.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_sync_cc.cc
)

add_executable(test-dvbt_rx ${test_dvbt_rx_sources})

target_link_libraries(
  test-dvbt_rx
  ${GNURADIO_RUNTIME_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CPPUNIT_LIBRARIES}
  gnuradio-dvbt_rx
)

GR_ADD_TEST(test_dvbt_rx test-dvbt_rx)
