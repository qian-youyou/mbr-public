
ADD_LIBRARY(http_utils http_connection_pool)

TARGET_LINK_LIBRARIES( http_utils event)




include_directories(~/local/include)

ADD_LIBRARY(utils SHARED router)

TARGET_LINK_LIBRARIES( utils ${GLOG_LIBRARY} ${GFLAGS_LIBRARY})

