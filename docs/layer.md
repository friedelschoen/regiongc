Region and GC
============================================================

The main principle of RegionGC is layered allocation. Every layer holds object which can be references by the same layer
or descendant. Layers can be seen as different scopes in other programming languages.

Objects
============================================================

Objects are blobs of arbitary data without any intentional structure. An object can resize dynamically and refer other
objects. As the location of a object may not be stable (its header and payload can be relocated), objects are refered
using an integer "the reference" which is resolved to the referenced object.

Layers
============================================================

Layers are stored on a stack, therefore there is a root layer (bottom-most layer) and an active layer (top-most layer).
A layer cannot be created in-between and non-active layers, a layer which does not sit on top, cannot be finished alone
but descendants must be finished first. There is a global option to automatically finish descendants first, but use it
with care to avoid losing objects unintentionally.

There are three types of layers:

Region Layers
------------------------------------------------------------

Region Layers stores its objects in the stack using simple bump-allocation. This means, when allocating an object, it is
stored on top of the stack, then the next object. When this layer finishes, all objects are free'ed very fast by
resetting the stacks top.

Due to nature of a stack, a non-active region layer cannot create new objects or change its objects sizes. To fix this
problem, new objects are temporary allocated on the active layer while not being the owner of this object. When this
layer finishes, those 'foreign object' are moved to its ancestor until it reaches its owner.

Garbage Collected Layers
------------------------------------------------------------

A Garbage Collected Layer stores its objects in dynamic heap using the installed allocator. Held objects can be
references by descendend layers to keep it alive, if no layer but itself holds a reference, this object is collected.
