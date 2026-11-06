#pragma once

#include "rgc-object.h"

#define RGC_REF_INDEX(r) ((r) & 0xffffff)
#define RGC_REF_GEN(r)   ((r) >> 24)

enum rgc_error rgc_ref_lookup(struct rgc_context *ctx, rgc_ref_t r, struct rgc_object **obj);

enum rgc_error rgc_ref_clear(struct rgc_context *ctx, rgc_ref_t r);
