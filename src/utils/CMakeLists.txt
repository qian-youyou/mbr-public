ADD_LIBRARY(utils router)

TARGET_LINK_LIBRARIES( utils
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})

