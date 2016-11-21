include_directories(~/local/include)
link_directories(~/local/lib)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

ADD_LIBRARY( arch SHARED simd_vector demangle cpuid simd exception exception_handler backtrace format fslock gpgpu environment_static cpu_info vm info rtti_utils rt)

TARGET_LINK_LIBRARIES( arch boost_system)
