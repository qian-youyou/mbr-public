include_directories(~/local/include)

ADD_LIBRARY(utils SHARED router)

TARGET_LINK_LIBRARIES( utils ${GLOG_LIBRARY} ${GFLAGS_LIBRARY})

