include_directories(~/local/include)
link_directories(~/local/lib)

ADD_SUBDIRECTORY(relay)
ADD_SUBDIRECTORY(banker)
ADD_SUBDIRECTORY(googleurl)
ADD_SUBDIRECTORY(utils)
ADD_SUBDIRECTORY(jml)
ADD_SUBDIRECTORY(soa)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

ADD_EXECUTABLE(master_banker_relay master_banker_relay)
TARGET_LINK_LIBRARIES( master_banker_relay
                    relay ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} event ${Boost_LIBRARIES})

ADD_EXECUTABLE(master_banker master_banker)
TARGET_LINK_LIBRARIES( master_banker banker arch types jml_utils carboncxx
                       ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} event ${Boost_LIBRARIES})
