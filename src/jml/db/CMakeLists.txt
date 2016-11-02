ADD_LIBRARY( db compact_size_types nested_archive.cc portable_iarchive.cc portable_oarchive.cc)

TARGET_LINK_LIBRARIES( db jml_utils)
