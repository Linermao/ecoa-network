{
  description = "Development shell for the ECOA LDP platform and bundled OpenDDS sources";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system:
          f (
            import nixpkgs {
              inherit system;
            }
          )
        );
    in
    {
      devShells = forAllSystems (
        pkgs:
        let
          lib = pkgs.lib;

          libraries = with pkgs; [
            apr
            log4cplus
            lttng-ust
            openssl
            rapidjson
            xercesc
            zlog
          ];

          devLibraries = map lib.getDev libraries;

          cmakePrefixPath =
            lib.makeSearchPath "lib/cmake" devLibraries + ":" + lib.makeSearchPath "share/cmake" devLibraries;

          pkgConfigPath =
            lib.makeSearchPath "lib/pkgconfig" devLibraries
            + ":"
            + lib.makeSearchPath "share/pkgconfig" devLibraries;
        in
        {
          default = pkgs.mkShell {
            packages =
              (with pkgs; [
                bear
                clang-tools
                cmake
                cmake-format
                cmake-language-server
                gcc
                gdb
                git
                gnumake
                ninja
                perl
                pkg-config
                python3
                cyclonedds
              ])
              ++ libraries;

            APR_INCLUDE_DIR = "${lib.getDev pkgs.apr}/include/apr-1";
            OPENDDS_RAPIDJSON = "${pkgs.rapidjson}";

            PLATFORM_CMAKE_FLAGS = lib.concatStringsSep " " [
              "-DLDP_LOG_USE=console"
              "-DLDP_LINK_TYPE=STATIC"
              "-DAPR_INCLUDE_DIR=${lib.getDev pkgs.apr}/include/apr-1"
              "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
            ];

            OPENDDS_CMAKE_FLAGS = lib.concatStringsSep " " [
              "-DOPENDDS_RAPIDJSON=${pkgs.rapidjson}"
              "-DOPENDDS_BUILD_TESTS=OFF"
              "-DBUILD_TESTING=OFF"
              "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
            ];

            shellHook = ''
              export DDS_ROOT="$PWD/OpenDDS"
              export LDP_LIB_ROOT="$PWD/lib"
              export CMAKE_GENERATOR=Ninja
              export CMAKE_PREFIX_PATH="${cmakePrefixPath}:''${CMAKE_PREFIX_PATH:-}"
              export PKG_CONFIG_PATH="${pkgConfigPath}:''${PKG_CONFIG_PATH:-}"
              export LD_LIBRARY_PATH="${lib.makeLibraryPath libraries}:''${LD_LIBRARY_PATH:-}"
              export NIX_CFLAGS_COMPILE="''${NIX_CFLAGS_COMPILE:-} -I$APR_INCLUDE_DIR"

              echo "ECOA/OpenDDS dev shell"
              echo "  OpenDDS source : $DDS_ROOT"
              echo "  lib source     : $LDP_LIB_ROOT"
              echo "  generator      : $CMAKE_GENERATOR"
              echo ""
              echo "OpenDDS configure example:"
              echo "  cmake -S OpenDDS -B build/opendds \$OPENDDS_CMAKE_FLAGS"
              echo ""
              echo "Platform CMake flags:"
              echo "  \$PLATFORM_CMAKE_FLAGS"
            '';
          };
        }
      );
    };
}
