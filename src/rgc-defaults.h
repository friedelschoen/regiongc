#pragma once

#ifndef RGC_SEGMENT_SIZE
#	define RGC_SEGMENT_SIZE 2048    // 2kB
#endif

#ifndef RGC_ALIGNMENT
#	define RGC_ALIGNMENT alignof(max_align_t)
#endif

#ifndef RGC_INITIAL_REFTABLE_SIZE
#	define RGC_INITIAL_REFTABLE_SIZE 32
#endif
