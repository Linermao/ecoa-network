# APR/APEX compatibility layer

`USE_APEX_API` is the single switch for this directory. When it is enabled,
all registered APR/APEX shim modules are compiled and their matching headers
redirect the supported APR types and functions to `apex_apr_*` definitions.

## Shim model

The shim include directory precedes the system APR directory. Each shim header
uses `#include_next` to retain the original APR declarations, then redirects
the supported API under `USE_APEX_API`. Its implementation lives in the
matching source file under `src/`.

The enabled modules currently cover time, status, general initialization,
pools, strings, mutexes and condition variables. Related scalar, socket, poll
and process/thread aliases are supplied by their matching shim headers.

There is intentionally no second "extended" feature flag. New modules must be
completed as a coherent header/source pair and added to `lib/CMakeLists.txt`
under the existing `USE_APEX_API` block.

## Transport boundary

`lib/apex_port/` is an LDP transport backend, not part of the APR replacement.
It is selected independently with `LDP_LOCAL_TRANSPORT=APEX` and should only
depend on the APEX queuing-port and time interfaces it directly uses.
