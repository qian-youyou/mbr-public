SET (GOOGLE_URL_SOURCES
	src/gurl
	src/url_canon_etc
	src/url_parse_file
	src/url_canon_mailtourl
	src/url_canon_icu
	src/url_canon_fileurl
	src/url_canon_pathurl
	src/url_parse
	src/url_canon_host
	src/url_canon_relative
	src/url_canon_ip
	src/url_util
	src/url_canon_filesystemurl
	src/url_canon_internal
	src/url_canon_stdurl
	src/url_canon_path
	src/url_canon_query)

ADD_LIBRARY( googleurl SHARED ${GOOGLE_URL_SOURCES})

TARGET_LINK_LIBRARIES( googleurl icui18n icuuc icudata m)

