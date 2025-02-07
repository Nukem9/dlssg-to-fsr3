<!-- @page page_ffx-api Introduction to FidelityFX API -->

<h1>Introduction to FidelityFX API</h1>

The FidelityFX API is a simple API created for small ABI surface and forwards-compatibility. It is delivered in Dynamic Link Library form and consists of 5 functions, declared in [`ffx_api.h`](../../ffx-api/include/ffx_api/ffx_api.h):

* `ffxCreateContext`
* `ffxDestroyContext`
* `ffxDispatch`
* `ffxQuery`
* `ffxConfigure`

Arguments are provided in a linked list of structs, each having a header with a struct type and pointer to next struct.

An application using the FidelityFX API must use one of the provided signed DLLs. This can be loaded at runtime with `LoadLibrary` and `GetProcAddress` (this is recommended) or at application startup using the dynamic linker via the .lib file.

Backend-specific functionality (for DirectX 12 or Vulkan) is only supported with the respective DLL.
Linking to both within the same application is not possible due to name resolution conflicts.

For convenience in C++ applications, helpers for initializing struct types correctly and linking headers are provided. Simply use the `.hpp` version of each header and replace the `ffx` prefix with the `ffx::` namespace.
Note that the helper functions wrapping API functions only work when linking using the .lib file. Using them with runtime loading will result in linker errors.

<h2>Descriptor structs</h2>

Functionality for effects is exposed using descriptor structs. Each struct has a header:

```C
typedef struct ffxApiHeader
{
    ffxStructType_t      type;  ///< The structure type. Must always be set to the corresponding value for any structure (found nearby with a similar name).
    struct ffxApiHeader* pNext; ///< Pointer to next structure, used for optional parameters and extensions. Can be null.
} ffxApiHeader;
```

Each descriptor has an associated struct type, usually defined directly before the struct declaration in the header.
The `type` field in the header must be set to that struct type. Failing to do this is undefined behavior and will likely result in crashes. The C++ headers (with extension `.hpp`) expose helpers for automatic initialization.

The `pNext` field is used to specify additional parameters and extensions in a linked list (or "chain"). Some calls require chaining certain other structs.

<h2>Context creation</h2>

Context creation is done by calling `ffxCreateContext`, which is declared as follows:

```C
ffxReturnCode_t ffxCreateContext(ffxContext* context, ffxCreateContextDescHeader* desc, const ffxAllocationCallbacks* memCb);
```

The `context` should be initalized to `null` before the call. Note that the second argument is a pointer to the struct header, not the struct itself. The third argument is used for custom allocators and may be `null`, see the [memory allocation section](#memory-allocation).

Example call:

```C
struct ffxCreateBackendDX12Desc createBackend = /*...*/;

struct ffxCreateContextDescUpscale createUpscale = { 0 };
createUpscale.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;

// fill struct ...

createUpscale.header.pNext = &createBackend.header;

ffxContext upscaleContext = NULL;
ffxReturnCode_t retCode = ffxContextCreate(&upscaleContext, &createUpscale.header, NULL);
// handle errors
```

Note that when using the C++ wrapper, the third argument comes second instead to support a variadic description argument.

```C++
// equivalent of above, chain of createUpscale and createBackend will be linked automatically.
ffxReturnCode_t retCode = ffx::ContextCreate(upscaleContext, nullptr, createUpscale, createBackend);
```

<h2>Context destruction</h2>

To destroy a context, call `ffxDestroyContext`:

```C
ffxReturnCode_t ffxDestroyContext(ffxContext* context, const ffxAllocationCallbacks* memCb);
```

The context will be `null` after the call. The memory callbacks must be compatible with the allocation callbacks used during context creation, see the [memory allocation section](#memory-allocation).

<h2>Query</h2>

To query information or resources from an effect, use `ffxQuery`:

```C
ffxReturnCode_t ffxQuery(ffxContext* context, ffxQueryDescHeader* desc);
```

The context must be a valid context created by `ffxCreateContext`, unless documentation for a specific query states otherwise.

Output values will be returned by writing through pointers passed in the query description.

<h2>Configure</h2>

To configure an effect, use `ffxConfigure`:

```C
ffxReturnCode_t ffxConfigure(ffxContext* context, const ffxConfigureDescHeader* desc);
```

The context must be a valid context created by `ffxCreateContext`, unless documentation for a specific configure description states otherwise.

All effects have a key-value configure struct used for simple options, for example:

```C
struct ffxConfigureDescUpscaleKeyValue
{
    ffxConfigureDescHeader  header;
    uint64_t                key;        ///< Configuration key, member of the FfxApiConfigureUpscaleKey enumeration.
    uint64_t                u64;        ///< Integer value or enum value to set.
    void*                   ptr;        ///< Pointer to set or pointer to value to set.
};
```

The available keys and constraints on values to pass are specified in the relevant technique's documentation.

<h2>Dispatch</h2>

To dispatch rendering commands or other functionality, use `ffxDispatch`:

```C
ffxReturnCode_t ffxDispatch(ffxContext* context, const ffxDispatchDescHeader* desc);
```

The context must be a valid context created by `ffxCreateContext`.

GPU rendering dispatches will encode their commands into a command list/buffer passed as input.

CPU dispatches will usually execute their function synchronously and immediately.

<h2>Resource structs</h2>

Resources like textures and buffers are passed to the FidelityFX API using the `FfxApiResource` structure.

For C++ applications, the backend headers define helper functions for constructing this from a native resource handle.

<h2>Version selection</h2>

FidelityFX API supports overriding the version of each effect on context creation. This is an optional feature. If version overrides are used, they **must** be used consistently:

* Only use version IDs returned by `ffxQuery` in `ffxQueryDescGetVersions` with the appropriate creation struct type
* Do not hard-code version IDs
* If calling `ffxQuery` without a context (`NULL` parameter), use the same version override as with `ffxCreateContext`
* The choice of version should either be left to the default behavior (no override) or the user (displayed in options UI)

Example version query using the C++ helpers:

```C++
ffx::QueryDescGetVersions versionQuery{};
versionQuery.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
versionQuery.device = GetDX12Device(); // only for DirectX 12 applications
uint64_t versionCount = 0;
versionQuery.outputCount = &versionCount;
// get number of versions for allocation
ffxQuery(nullptr, &versionQuery.header);

std::vector<const char*> versionNames;
std::vector<uint64_t> versionIds;
m_FsrVersionIds.resize(versionCount);
versionNames.resize(versionCount);
versionQuery.versionIds = versionIds.data();
versionQuery.versionNames = versionNames.data();
// fill version ids and names arrays.
ffxQuery(nullptr, &versionQuery.header);
```

<h2>Error handling</h2>

All calls to the FidelityFX API return a value of type `ffxReturnCode_t`. If using the C++ wrapper, this is replaced by `ffx::ReturnCode`.

A successful operation will return `FFX_API_RETURN_OK`. All other return codes are errors. Future versions may add new return codes not yet present in the headers.

Applications should be able to handle errors gracefully, even if it is unrecoverable.

<h2>Memory allocation</h2>

To control memory allocations, pass a pointer to `ffxAllocationCallbacks` to `ffxCreateContext` and `ffxDestroyContext`.

If a null pointer is passed, then global `malloc` and `free` will be used.

For custom allocators, two functions are required:

```C
// Memory allocation function. Must return a valid pointer to at least size bytes of memory aligned to hold any type.
// May return null to indicate failure. Standard library malloc fulfills this requirement.
typedef void* (*ffxAlloc)(void* pUserData, uint64_t size);

// Memory deallocation function. May be called with null pointer as second argument.
typedef void (*ffxDealloc)(void* pUserData, void* pMem);

typedef struct ffxAllocationCallbacks
{
    void* pUserData;
    ffxAlloc alloc;
    ffxDealloc dealloc;
} ffxAllocationCallbacks;
```

`pUserData` is passed through to the callbacks without modification or validation. FidelityFX API code will not attempt to dereference it, nor will it store it.

If a custom allocator is used for context creation, a compatible struct must be used for destruction. That means that any pointer allocated with the callbacks and user data during context creation must be deallocatable using the callback and user data passed to `ffxDestroyContext`.
