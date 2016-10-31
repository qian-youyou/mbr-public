ADD_LIBRARY(banker banker)

TARGET_LINK_LIBRARIES( banker utils
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})


