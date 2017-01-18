include_directories(~/local/include)
SET(JS_SOURCES
    js_wrapped
	js_value
	js_utils
	js_registry
	js_call)

ADD_LIBRARY( js ${JS_SOURCES})

TARGET_LINK_LIBRARIES( js jsoncpp node-v8 arch utils types)

