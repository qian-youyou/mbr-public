include_directories(~/local/include)
link_directories(~/local/lib)

ADD_LIBRARY( jsoncpp SHARED json_reader json_writer json_value)

TARGET_LINK_LIBRARIES( jsoncpp boost_iostreams)
