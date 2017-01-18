include_directories(~/local/include)

ADD_LIBRARY(relay SHARED relay)

TARGET_LINK_LIBRARIES( relay
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES} http_utils)

