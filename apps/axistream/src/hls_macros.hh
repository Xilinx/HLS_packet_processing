#pragma once

#define HLS_PRAGMA_SUB(x) _Pragma (#x)
#ifdef __SYNTHESIS__
#define HLS_PRAGMA(x) HLS_PRAGMA_SUB(x)
#else
#define HLS_PRAGMA(x)
#endif
