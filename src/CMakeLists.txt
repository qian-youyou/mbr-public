#ADD_SUBDIRECTORY(rabbit)

ADD_EXECUTABLE(master_banker_relay master_banker_relay)
TARGET_LINK_LIBRARIES( master_banker_relay
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} event ${Boost_LIBRARIES})
