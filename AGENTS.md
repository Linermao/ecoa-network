# Repository Guidance

This repository currently has two parallel development tracks:

- APR-to-APEX compatibility work under `lib/apex_apr/`
- LDP transport work, including TCP/UDP and the newer DDS backend under `lib/dds/`

Keep these tracks separate unless a change explicitly needs to touch both. APR/APEX compatibility changes should not casually refactor DDS behavior, and DDS transport changes should not reshape the APR shim.

## APR/APEX Compatibility Model

The APR compatibility layer is an incremental shim, not a full APR replacement yet.

The current model is:

1. Add a header in `lib/apex_apr/include/` with the same basename as the APR header being intercepted.
2. In that header, use `#include_next <apr_xxx.h>` so the original APR declarations remain visible.
3. Under `#ifdef USE_APEX_API`, declare wrapper functions named `apex_apr_<original_name>`.
4. Remap the original APR function name with a macro, for example:

```c
#define apr_sleep apex_apr_sleep
```

5. Implement the wrapper in the matching source file under `lib/apex_apr/src/`.
6. In the implementation file, `#undef` the remapped APR function before defining the wrapper, then either delegate to system APR or call the future APEX implementation.

The current demonstration intercepts `apr_sleep`, which belongs to `apr_time.h`:

- `lib/apex_apr/include/apr_time.h`
- `lib/apex_apr/src/apr_time.c`

At this stage the wrapper prints a trace line and delegates to system APR. This is intentional: it proves include ordering, macro remapping, source compilation, and runtime interception before replacing behavior with APEX calls.

## Adding More APR Replacements

Follow APR's module boundaries. If APR declares a function in `apr_thread_mutex.h`, add or edit:

- `lib/apex_apr/include/apr_thread_mutex.h`
- `lib/apex_apr/src/apr_thread_mutex.c`

If APR declares a function in `apr_pools.h`, add or edit:

- `lib/apex_apr/include/apr_pools.h`
- `lib/apex_apr/src/apr_pools.c`

When adding a new source file, also add it to the `USE_APEX_API` `target_sources` block in `lib/CMakeLists.txt`.

Prefer one APR module per shim header/source pair. Avoid collecting unrelated replacements in a generic `apr.c`; it makes the future APEX migration harder to audit.

## Function Replacement Checklist

For each new APR function wrapper:

- Confirm which APR header originally declares it.
- Create or update the matching shim header in `lib/apex_apr/include/`.
- Keep `#include_next <apr_xxx.h>` at the top of the shim header.
- Declare `apex_apr_<function>` with the same signature.
- Add `#define apr_<function> apex_apr_<function>` under `USE_APEX_API`.
- Implement the wrapper in the matching source file.
- In the source file, include the shimmed APR header and then `#undef apr_<function>`.
- During the transition, use a small trace plus delegation to system APR.
- Later, replace delegation with the actual APEX call when the behavior is understood.

## Type And Structure Replacement

Be cautious with APR types.

Opaque APR pointer types are the easiest to replace because project code normally passes pointers around without reading fields. For these, define a compatible shim type and implement the associated create/destroy/use functions together.

Avoid changing public LDP structure fields unless the task explicitly requires it. APR types appear in public headers such as `ldp_structures.h` and `ldp_network.h`; changing them can affect generated platform code.

Do not remap APR types with simple macros next to function remaps unless the compatibility story is clear. Prefer a deliberate module-level replacement.

## Build Configuration

Central project defaults live in `cmake_config.cmake`.

Current defaults:

- `USE_APEX_API=ON`
- `LDP_LOCAL_TRANSPORT=TCP`

`lib/CMakeLists.txt` consumes these settings. When `USE_APEX_API` is enabled, it adds `lib/apex_apr/include` before the normal APR include path and compiles the shim sources.

Developers can still override these from CMake, for example:

```bash
cmake -S example -B build/example \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DUSE_APEX_API=OFF \
  -DLDP_LOCAL_TRANSPORT=DDS
```

## DDS Transport Work

DDS support is a separate transport backend. Its code lives primarily under `lib/dds/`, and it is selected through `LDP_LOCAL_TRANSPORT=DDS`.

Do not change DDS packet format, CycloneDDS setup, or DDS routing while working on the APR/APEX shim unless the user asks for both. Conversely, DDS work should preserve the APR shim include and CMake structure unless that work explicitly needs to adjust build configuration.
