ADD_LIBRARY( jml_utils environment file_functions filter_streams string_functions parse_context configuration csv exc_check	exc_assert hex_dump	lzma lz4 lz4hc floating_point json_parsing rng abort)

TARGET_LINK_LIBRARIES( jml_utils ACE arch ${Boost_LIBRARIES} lzma)
