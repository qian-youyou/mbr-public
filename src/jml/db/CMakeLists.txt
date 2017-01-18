include_directories(~/local/include)
link_directories(~/local/lib)

ADD_LIBRARY( db SHARED compact_size_types nested_archive portable_iarchive portable_oarchive)

TARGET_LINK_LIBRARIES( db jml_utils)
