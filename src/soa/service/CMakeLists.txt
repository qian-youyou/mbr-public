include_directories(~/local/include)
link_directories(~/local/lib)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

ADD_LIBRARY( services SHARED redis)

TARGET_LINK_LIBRARIES( services jml_utils hiredis
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})
