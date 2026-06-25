# universal-wasm-loader-c — vcpkg port
#
# The loader itself is a single header, but it links against the wasmtime C API.
# vcpkg has no `wasmtime` port (wasmtime ships as prebuilt GitHub release
# artifacts), so this port downloads the matching wasmtime C API SDK for the
# active triplet and installs its headers + library alongside our header. A
# CMake config exposes an INTERFACE target wiring the include dir, the wasmtime
# library, and the platform system libraries.
#
# Bumping versions:
#   * loader  — UWL_VERSION + UWL_HEADER_SHA512 (sha512 of the raw header blob at the tag)
#   * wasmtime — WASMTIME_VERSION + the per-arch _wt_sha values

set(UWL_VERSION "1.1.0")
set(UWL_HEADER_SHA512 "45701c61d2069a71685159e87d6a9c58ad365070aadab17b0d6cd3a7f9d71d2ec014a00e94d1909c60223c136360eaadc6c08571a77b191d9ceb1dc6f2e3fa53")
set(UWL_LICENSE_SHA512 "7e9713dd3e9b85cf838c41c24d47dd2e96ec61414d02de0f1c4e77aedbefa2c2cab30e3f7230973bb1f49a62b7b20732ba4f5e265f399f330d3993158443c97e")

set(WASMTIME_VERSION "45.0.2")
set(WASMTIME_BASE "https://github.com/bytecodealliance/wasmtime/releases/download/v${WASMTIME_VERSION}")

# ── select the wasmtime C API artifact for this triplet ──────────────────────
if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
  set(_wt_triple "x86_64-windows")
  set(_wt_ext "zip")
  set(_wt_sha "0deff10840195f1fc38c0cbf24f36461eff8984069ea4e7c3e05b666d8fdbf86672b18254ea1959e512784b81f57e4008eaeba0dae29d2913a208e21f55f2e7e")
elseif(VCPKG_TARGET_IS_MINGW)
  set(_wt_triple "x86_64-mingw")
  set(_wt_ext "zip")
  set(_wt_sha "a298e7dc562cdc6ab149934823f2cf7b465e604a1f8c2d8cbfaa396c16cd6b3f60eac6d51c571829a93c7c58fe6cf9caf259db2be8ec53d0634f6685f7473d20")
elseif(VCPKG_TARGET_IS_LINUX)
  if(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(_wt_triple "aarch64-linux")
    set(_wt_sha "e5093abb5bc59216b20b7f770e83094778d0f1d09619baaf575c84cbaad7216affb106f0f9a0e84121e3391f7d1892c14e01cde4b9a04772aeec29103ab7fda1")
  else()
    set(_wt_triple "x86_64-linux")
    set(_wt_sha "65112f97a836001eb64525c18cae1a8ea9c058fd02c61bce9b4c3e1941b6575af09be3903105ec01e27a1eb8ad0e9fa54eeeebdafe4655e67fb90df520be2f3e")
  endif()
  set(_wt_ext "tar.xz")
elseif(VCPKG_TARGET_IS_OSX)
  if(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(_wt_triple "aarch64-macos")
    set(_wt_sha "6d53d75eead924d62e92b735ccafd523506bfb2520f7116b1c35248aadb5c1a76c1595bf0f9731edb676a4d93ea34062eeb7c6e29b22bc0f924272edce52ee22")
  else()
    set(_wt_triple "x86_64-macos")
    set(_wt_sha "930ed0c07e8a4a6ac8338f8035a787c2a3180f351d355a2c326681248a96b5bfd19c85a8f6883b1fd781f2553e169e64a3f8ebb24491d7b47a4a27ec7f0c52da")
  endif()
  set(_wt_ext "tar.xz")
else()
  message(FATAL_ERROR "universal-wasm-loader-c: unsupported triplet ${TARGET_TRIPLET}")
endif()

set(_wt_name "wasmtime-v${WASMTIME_VERSION}-${_wt_triple}-c-api")

vcpkg_download_distfile(WASMTIME_ARCHIVE
  URLS "${WASMTIME_BASE}/${_wt_name}.${_wt_ext}"
  FILENAME "${_wt_name}.${_wt_ext}"
  SHA512 "${_wt_sha}"
)
vcpkg_extract_source_archive(WASMTIME_SRC ARCHIVE "${WASMTIME_ARCHIVE}" NO_REMOVE_ONE_LEVEL)
set(_wt_root "${WASMTIME_SRC}/${_wt_name}")
if(NOT EXISTS "${_wt_root}/include")
  set(_wt_root "${WASMTIME_SRC}")
endif()

# ── headers: wasmtime C API + our loader ─────────────────────────────────────
file(INSTALL "${_wt_root}/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

vcpkg_download_distfile(UWL_HEADER
  URLS "https://raw.githubusercontent.com/jrmarcum/universalWasmLoader-c/v${UWL_VERSION}/universal_wasm_loader.h"
  FILENAME "universal_wasm_loader-${UWL_VERSION}.h"
  SHA512 "${UWL_HEADER_SHA512}"
)
file(INSTALL "${UWL_HEADER}" DESTINATION "${CURRENT_PACKAGES_DIR}/include" RENAME "universal_wasm_loader.h")

# ── wasmtime library payload (release + debug copies) ────────────────────────
function(uwl_install_libs _dst)
  file(MAKE_DIRECTORY "${_dst}/lib")
  if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    if(VCPKG_TARGET_IS_WINDOWS)
      file(MAKE_DIRECTORY "${_dst}/bin")
      file(GLOB _dlls "${_wt_root}/lib/*.dll")
      if(_dlls)
        file(INSTALL ${_dlls} DESTINATION "${_dst}/bin")
      endif()
      file(GLOB _implibs "${_wt_root}/lib/*.dll.lib" "${_wt_root}/lib/*.dll.a")
      if(_implibs)
        file(INSTALL ${_implibs} DESTINATION "${_dst}/lib")
      endif()
    else()
      file(GLOB _shared "${_wt_root}/lib/*.so" "${_wt_root}/lib/*.so.*" "${_wt_root}/lib/*.dylib")
      if(_shared)
        file(INSTALL ${_shared} DESTINATION "${_dst}/lib")
      endif()
    endif()
  else() # static
    if(VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_MINGW)
      if(EXISTS "${_wt_root}/lib/wasmtime.lib")
        file(INSTALL "${_wt_root}/lib/wasmtime.lib" DESTINATION "${_dst}/lib")
      endif()
    else()
      if(EXISTS "${_wt_root}/lib/libwasmtime.a")
        file(INSTALL "${_wt_root}/lib/libwasmtime.a" DESTINATION "${_dst}/lib")
      endif()
    endif()
  endif()
endfunction()

uwl_install_libs("${CURRENT_PACKAGES_DIR}")
uwl_install_libs("${CURRENT_PACKAGES_DIR}/debug")

# ── platform system libraries the wasmtime static archive needs ──────────────
if(VCPKG_TARGET_IS_WINDOWS)
  set(UWL_SYSLIBS_CMAKE "ws2_32;bcrypt;userenv;ntdll;ole32;shlwapi;advapi32;kernel32;uuid")
elseif(VCPKG_TARGET_IS_OSX)
  set(UWL_SYSLIBS_CMAKE "pthread;-framework CoreFoundation;-framework Security")
else()
  set(UWL_SYSLIBS_CMAKE "pthread;dl;m")
endif()

# ── CMake package config ─────────────────────────────────────────────────────
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/${PORT}-config.cmake.in"
  "${CURRENT_PACKAGES_DIR}/share/${PORT}/${PORT}-config.cmake"
  @ONLY
)

# ── copyright + usage ────────────────────────────────────────────────────────
vcpkg_download_distfile(UWL_LICENSE
  URLS "https://raw.githubusercontent.com/jrmarcum/universalWasmLoader-c/v${UWL_VERSION}/LICENSE"
  FILENAME "universal-wasm-loader-c-LICENSE-${UWL_VERSION}"
  SHA512 "${UWL_LICENSE_SHA512}"
)
vcpkg_install_copyright(FILE_LIST "${UWL_LICENSE}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
