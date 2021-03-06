
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7 FATAL_ERROR)

INCLUDE(FindPackageHandleStandardArgs)

FIND_LIBRARY(GFLAGS_LIBRARY gflags)
FIND_PATH(GFLAGS_INCLUDE_DIR "glog/logging.h")

SET(GFLAGS_LIBRARIES ${GFLAGS_LIBRARY})

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Gflags
  REQUIRED_ARGS GFLAGS_INCLUDE_DIR GFLAGS_LIBRARIES)
