include_directories(~/local/include)
link_directories(~/local/lib)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

SET(BANKER_UTILS_SOURCES
   currency
   account
   account_key)

ADD_LIBRARY(banker_utils SHARED ${BANKER_UTILS_SOURCES})

TARGET_LINK_LIBRARIES( banker_utils types jsoncpp value_description
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})


ADD_LIBRARY(banker SHARED banker)

TARGET_LINK_LIBRARIES( banker utils banker_utils jml_utils types jsoncpp services
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})


