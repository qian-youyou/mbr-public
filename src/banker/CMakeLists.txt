include_directories(~/local/include)
link_directories(~/local/lib)

SET(BANKER_UTILS_SOURCES
   currency
   account
   account_key)

ADD_LIBRARY(banker_utils SHARED ${BANKER_UTILS_SOURCES})

ADD_LIBRARY(banker SHARED banker)

TARGET_LINK_LIBRARIES( banker utils banker_utils jml_utils types jsoncpp 
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})


