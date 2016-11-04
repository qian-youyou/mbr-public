include_directories(~/local/include)
link_directories(~/local/lib)

ADD_LIBRARY( types SHARED date localdate string id url periodic_utils csiphash)

TARGET_LINK_LIBRARIES( types boost_regex boost_date_time
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES}
                     ACE db googleurl cityhash jml_utils)

SET(VALUE_DESCRIPTION_SOURCES
    value_description
    json_parsing
    json_printing
    periodic_utils_value_descriptions)

ADD_LIBRARY( value_description SHARED ${VALUE_DESCRIPTION_SOURCES})

TARGET_LINK_LIBRARIES( value_description arch types
                    ${GLOG_LIBRARY} ${GFLAGS_LIBRARY} ${Boost_LIBRARIES})
