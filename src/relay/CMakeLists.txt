ADD_LIBRARY(relay relay)

TARGET_LINK_LIBRARIES( relay
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})

