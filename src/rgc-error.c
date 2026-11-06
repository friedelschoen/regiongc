#include <regiongc.h>

const char *rgc_error_string(enum rgc_error err) {
	switch (err) {
		case RGC_OK:
			return "Success";
		case RGC_ERR_NO_MEMORY:
			return "Out of memory";
		case RGC_ERR_NOT_TOP:
			return "Layer has active children";
		case RGC_ERR_PINNED:
			return "Object is pinned";
		case RGC_ERR_NULL_REF:
			return "Null reference";
		case RGC_ERR_INVAL_REF:
			return "Invalid or expired reference";
		case RGC_ERR_LIFETIME:
			return "Reference violates lifetime constraints";
		case RGC_ERR_NOT_FOUND:
			return "Not found";
		case RGC_ERR_RO_LAYER:
			return "Layer is read-only";
		default:
			return "Unknown error";
	}
}
