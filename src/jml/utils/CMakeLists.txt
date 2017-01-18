include_directories(~/local/include)
link_directories(~/local/lib)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

SET(JML_UTILS_SOURCES
    environment
    file_functions
    string_functions
    parse_context
    configuration
    filter_streams
    lzma
    xxhash
    lz4
    lz4hc
    csv
    exc_check
    exc_assert
    hex_dump    
    floating_point
    json_parsing
    rng
    abort)



ADD_LIBRARY( jml_utils SHARED ${JML_UTILS_SOURCES})

TARGET_LINK_LIBRARIES( jml_utils ACE arch boost_iostreams lzma boost_thread cryptopp)
