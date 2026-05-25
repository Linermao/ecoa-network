# DDS Smoke Example

This example builds the local LDP library with the backend selected in
`../cmake_config.cmake` and
calls the two protocol entry points currently implemented by the DDS skeleton:

- `ldp_start_father_server`
- `ldp_start_comp_server`

Configure and run it from the repository root:

```bash
cmake -S example -B build/example-dds \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR"
cmake --build build/example-dds
./build/example-dds/ldp_dds_smoke
```

Expected output while the DDS transport is still a skeleton:

```text
[DDS] main server skeleton reached; DDS wait/read loop is not implemented
[DDS] component server skeleton reached; DDS wait/read loop is not implemented
```
