#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup errors Error Codes
 * @{
 */

/** Result codes returned by regiongc operations. */
enum rgc_error {
	RGC_OK,            /**< Operation completed successfully. */
	RGC_ERR_NO_MEMORY, /**< Memory allocation failed. */
	RGC_ERR_NOT_TOP,   /**< Layer cannot be finished while child layers are active. */
	RGC_ERR_PINNED,    /**< Object cannot be moved or destroyed while pinned. */
	RGC_ERR_NULL_REF,  /**< Attempted to access RGC_NOREF. */
	RGC_ERR_INVAL_REF, /**< Reference is invalid, expired, or has a mismatched generation. */
	RGC_ERR_LIFETIME,  /**< Reference violates the layer lifetime hierarchy. */
	RGC_ERR_NOT_FOUND, /**< Requested reference or relationship was not found. */
	RGC_ERR_RO_LAYER,  /**< Operation is not permitted on a read-only layer. */
};

/**
 * Returns a human-readable description of an error code.
 *
 * The returned string has static storage duration and must not be freed
 * or modified by the caller.
 *
 * @param[in] err Error code to describe.
 * @return Null-terminated, statically allocated error string.
 */
const char *rgc_error_string(enum rgc_error err);

/** @} */

/**
 * @defgroup context Context
 * @{
 */

/** Flags controlling context-wide behaviour. */
enum {
	RGC_CASCADE = 1 << 0, /**< Automatically finish descendant layers before their parent. */
};

/**
 * Regiongc runtime context.
 *
 * A context owns the reference table, manages collection state, and contains
 * the root layer. All references and layers belong to a single context.
 *
 * The structure is opaque; its internal state cannot be accessed directly.
 */
struct rgc_context;

/**
 * Allocator callback used for dynamic memory management.
 *
 * The allocator is responsible for allocating, resizing, and freeing
 * memory used by segment stacks, reference tables, and GC objects.
 *
 * The following conventions apply:
 * - If @p ptr is NULL, allocate @p newsize bytes.
 * - If @p newsize is zero, free @p ptr.
 * - Otherwise, resize @p ptr from @p oldsize to @p newsize bytes.
 *
 * A successful resize preserves the existing contents up to the smaller
 * of the old and new sizes. On failure, the original allocation must
 * remain valid.
 *
 * Freshly allocated memory need not be initialized.
 *
 * @param[in]    ptr     Existing allocation, or NULL for a new allocation.
 * @param[in]    oldsize Size of the existing allocation in bytes.
 * @param[in]    newsize Requested allocation size in bytes.
 * @param[inout] userdata Optional userdata which user supplies in constructor.
 * @return Pointer to the allocated buffer, or NULL on failure.
 */
typedef void *(*rgc_allocate_t)(void *ptr, size_t oldsize, size_t newsize, void *userdata);

/**
 * Creates a new regiongc context.
 *
 * Initializes the reference table, internal allocation structures, and
 * permanent root layer.
 *
 * @param[in]    alloc Allocator callback, or NULL to use the default allocator.
 * @param[inout] userdata Optional userdata which is passed to alloc.
 * @param[in]    flags Bitwise combination of context flags.
 * @param[out]   ctx   Receives the newly created context.
 * @return RGC_OK on success, or an error code on failure.
 */
enum rgc_error rgc_context_new(rgc_allocate_t alloc, void *userdata, int flags, struct rgc_context **ctx);

/**
 * Returns the permanent root layer of a context.
 *
 * The root layer is the ancestor of all layers created within the context.
 * It cannot be finished and does not permit ordinary object allocation.
 *
 * @param[in] ctx Initialized regiongc context.
 * @return Pointer to the root layer.
 */
struct rgc_layer *rgc_context_layer(struct rgc_context *ctx);

/**
 * Destroys a regiongc context and releases all associated resources.
 *
 * All layers, managed objects, reference entries, and internal allocations
 * owned by the context are destroyed. Registered finalizers are invoked
 * for objects that have not yet been finalized.
 *
 * After successful destruction, all references, layer pointers, and borrowed
 * payload pointers associated with the context become invalid.
 *
 * @param[in] ctx Context to destroy.
 *
 * @warning The context pointer must not be used after successful destruction.
 */
void rgc_context_destroy(struct rgc_context *ctx);

/** @} */

/**
 * @defgroup layers Layers
 * @{
 */

/**
 * A logical lifetime scope for managed objects.
 *
 * The structure is opaque; its internal state cannot be accessed directly.
 */
struct rgc_layer;

/** Flags controlling the behaviour of a layer. */
enum {
	RGC_LAYER_GC = 1 << 0,    /**< Use tracing garbage collection for this layer. */
	RGC_LAYER_FINAL = 1 << 1, /**< Reuse this layer for nested scopes instead of creating children. */
};

/**
 * Creates a child layer.
 *
 * The new layer inherits the context of its parent. By default, objects
 * are managed using region-based allocation. Specify RGC_LAYER_GC to
 * create a garbage-collected layer instead.
 *
 * Multiple child layers may coexist as independent branches.
 *
 * If the parent is a final layer, no new layer is allocated. Instead,
 * the parent's nesting counter is incremented and the parent itself
 * is returned.
 *
 * @param[in]  parent Parent layer.
 * @param[in]  flags  Bitwise combination of layer flags.
 * @param[out] layer  Receives the created or reused layer.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @note A layer must not outlive its parent.
 */
enum rgc_error rgc_layer_new(
    struct rgc_layer *parent,
    int flags,
    struct rgc_layer **layer);

/**
 * Finishes a layer and releases its resources.
 *
 * Objects owned by the layer are destroyed unless they have been
 * transferred to a longer-lived layer.
 *
 * Region storage is reclaimed when the layer finishes. GC-managed
 * objects are reclaimed according to the lifetime and collection
 * rules of their containing layer.
 *
 * A layer with active children cannot be finished unless RGC_CASCADE
 * was enabled on its context. In that case, descendants are finished
 * before the requested layer.
 *
 * Finishing a final layer first decreases its nesting counter.
 * The layer is destroyed only when no nested uses remain.
 *
 * @param[in] layer Layer to finish.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @retval RGC_ERR_NOT_TOP Active descendants prevent the operation.
 * @retval RGC_ERR_PINNED An object cannot be released while pinned.
 *
 * @warning The layer pointer becomes invalid after successful destruction.
 */
enum rgc_error rgc_layer_finish(struct rgc_layer *layer);

/**
 * Performs garbage collection within a layer subtree.
 *
 * Traverses descendant layers before their ancestors. Region objects
 * act as sources of strong references, while GC layers sweep objects
 * that are not reachable through the managed reference graph.
 *
 * Weak references do not keep their target objects alive.
 *
 * @param[in] l Root of the subtree to collect.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @note Only the specified layer and its descendants are traversed.
 * Ancestor and sibling subtrees are not collected.
 */
enum rgc_error rgc_layer_collect(struct rgc_layer *l);

/** @} */

/**
 * @defgroup references References
 * @{
 */

/**
 * Stable handle identifying a managed object.
 *
 * References use a 32-bit representation consisting of a table index
 * and generation counter. The handle remains unchanged when an object
 * is relocated or promoted.
 *
 * A reference does not itself keep an object alive. Reachability is
 * determined by strong references registered in the object graph.
 *
 * Once an object is destroyed, its reference becomes invalid.
 */
typedef uint32_t rgc_ref_t;

/** Null reference, identifying no managed object. */
#define RGC_NOREF ((rgc_ref_t) 0)

/**
 * Callback invoked when a managed object is finalized.
 *
 * The object is still accessible during the callback, but is being
 * destroyed. The callback should release resources associated with
 * the object rather than attempt to extend its lifetime.
 *
 * @param[in] ctx      Context owning the object.
 * @param[in] ref      Reference identifying the object being finalized.
 * @param[in] userdata User-defined data supplied during registration.
 */
typedef void (*rgc_finalizer_t)(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    void *userdata);

/**
 * Allocates a new managed object.
 *
 * Creates an object consisting of an internal header and a payload
 * containing @p size bytes. The payload is initially zero-initialized.
 *
 * The allocation strategy is determined by the containing layer.
 * The returned reference remains stable even if the object moves.
 *
 * @param[in]  l    Layer owning the new object.
 * @param[in]  size Payload size in bytes.
 * @param[out] out  Receives the new object reference.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @retval RGC_ERR_NO_MEMORY Allocation failed.
 * @retval RGC_ERR_RO_LAYER The selected layer does not permit allocation.
 */
enum rgc_error rgc_ref_new(
    struct rgc_layer *l,
    size_t size,
    rgc_ref_t *out);

/**
 * Registers or replaces the finalizer of an object.
 *
 * The finalizer is invoked when the object is destroyed, either by
 * region cleanup or garbage collection.
 *
 * @param[in] ctx       Context owning the reference.
 * @param[in] ref       Object reference.
 * @param[in] finalizer Finalizer callback, or NULL to remove it.
 * @param[in] userdata  User-defined callback argument.
 * @return RGC_OK on success, or an error code on failure.
 */
enum rgc_error rgc_ref_set_finalizer(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    rgc_finalizer_t finalizer,
    void *userdata);

/**
 * Pins an object and borrows its payload buffer.
 *
 * Prevents the object from being relocated or destroyed until the
 * corresponding rgc_ref_unpin() call.
 *
 * Multiple pins may coexist. Each successful pin must be matched
 * by an unpin operation.
 *
 * @param[in]  ctx     Context owning the reference.
 * @param[in]  ref     Object reference.
 * @param[out] payload Receives a pointer to the object's payload.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @warning The returned pointer must not be accessed after unpinning.
 */
enum rgc_error rgc_ref_pin(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    void **payload);

/**
 * Releases a previously acquired object pin.
 *
 * Decrements the object's pin count. When the count reaches zero,
 * the object may once again be relocated or destroyed.
 *
 * @param[in] ctx Context owning the reference.
 * @param[in] ref Previously pinned object reference.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @note Every successful rgc_ref_pin() requires one rgc_ref_unpin().
 */
enum rgc_error rgc_ref_unpin(
    struct rgc_context *ctx,
    rgc_ref_t ref);

/**
 * Registers a reference edge between two managed objects.
 *
 * The source object @p ref is recorded as referencing @p other.
 * Strong edges contribute to GC reachability; weak edges do not.
 *
 * The target must belong to the same layer as the source or to
 * one of its ancestor layers.
 *
 * This operation modifies the managed reference graph. It does not
 * write the reference into the source object's payload.
 *
 * @param[in] ctx   Context containing both objects.
 * @param[in] ref   Source object reference.
 * @param[in] other Target object reference.
 * @param[in] weak  True for a weak edge, false for a strong edge.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @retval RGC_ERR_LIFETIME The target violates lifetime constraints.
 * @retval RGC_ERR_NO_MEMORY Reference graph allocation failed.
 *
 * @note Multiple edges to the same target may be registered.
 */
enum rgc_error rgc_ref_link(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    rgc_ref_t other,
    bool weak);

/**
 * Removes a registered reference edge.
 *
 * Removes one edge from @p ref to @p other. If multiple matching
 * edges exist, only the first match is removed.
 *
 * The relative order of remaining edges is not preserved.
 *
 * @param[in] ctx   Context containing both objects.
 * @param[in] ref   Source object reference.
 * @param[in] other Target object reference.
 * @return RGC_OK on success, or an error code on failure.
 *
 * @retval RGC_ERR_NOT_FOUND No matching edge exists.
 */
enum rgc_error rgc_ref_unlink(
    struct rgc_context *ctx,
    rgc_ref_t ref,
    rgc_ref_t other);

/**
 * Checks whether a reference identifies a live object.
 *
 * Validates the reference index, generation, and associated object.
 *
 * @param[in] ctx Context owning the reference table.
 * @param[in] r   Reference to validate.
 * @return RGC_OK if the reference is valid, otherwise an error code.
 *
 * @retval RGC_ERR_NULL_REF The reference is RGC_NOREF.
 * @retval RGC_ERR_INVAL_REF The reference no longer identifies a live object.
 */
enum rgc_error rgc_ref_valid(
    struct rgc_context *ctx,
    rgc_ref_t r);


/**
 * Size in bytes a stack-object can be.
 *
 * @note This only affects object held by a region-layer,
 * garbage-collected objects can be sized arbirary.
 */
extern size_t rgc_object_size_max;

/** @} */
