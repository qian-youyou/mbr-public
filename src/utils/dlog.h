#include <glog/logging.h>
#ifdef VERBOSELOG
#define DLOGINFO(stream) LOG(INFO)<<stream
#else
#define DLOGINFO(stream) ;
#endif
