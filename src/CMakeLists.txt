ADD_SUBDIRECTORY(relay)
ADD_SUBDIRECTORY(utils)

ADD_EXECUTABLE(master_banker_relay master_banker_relay)
TARGET_LINK_LIBRARIES( master_banker_relay
                    relay ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} event ${Boost_LIBRARIES})
