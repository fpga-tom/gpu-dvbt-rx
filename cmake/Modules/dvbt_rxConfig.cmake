INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_DVBT_RX dvbt_rx)

FIND_PATH(
    DVBT_RX_INCLUDE_DIRS
    NAMES dvbt_rx/api.h
    HINTS $ENV{DVBT_RX_DIR}/include
        ${PC_DVBT_RX_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    DVBT_RX_LIBRARIES
    NAMES gnuradio-dvbt_rx
    HINTS $ENV{DVBT_RX_DIR}/lib
        ${PC_DVBT_RX_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DVBT_RX DEFAULT_MSG DVBT_RX_LIBRARIES DVBT_RX_INCLUDE_DIRS)
MARK_AS_ADVANCED(DVBT_RX_LIBRARIES DVBT_RX_INCLUDE_DIRS)

