include_directories(~/local/include)
link_directories(~/local/lib)

ADD_LIBRARY( services SHARED redis)

TARGET_LINK_LIBRARIES( services jml_utils hiredis
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})
