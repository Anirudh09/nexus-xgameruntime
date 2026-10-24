# Nexus XGameRuntime

An independent, unofficial fork of [Xodus XGameRuntime](https://github.com/xodus-gaming/xgameruntime), based on its C++ implementation. It is not an official Xodus or Microsoft project.

This implementation is under development. Threading and desktop system interfaces are available. Authentication, cloud saves, package management and Store interfaces are incomplete. Do not replace an installed Nexus runtime with this component.

## Building

Linux cross-compilation requires CMake 3.20 or later, Make, MinGW-w64 with C++23 support, and Wine's WIDL. Tests also require Node.js 22, Wine with both Windows architectures enabled, and the pinned GoogleTest submodule.

```sh
git submodule update --init --recursive
cmake -S . -B build/x64 -DCMAKE_TOOLCHAIN_FILE=win64-target.cmake -DENABLE_TESTS=ON -DNEXUS_MIGRATION_PROBES=ON
cmake --build build/x64 --parallel
node scripts/test-component.mjs build/x64
```

Use `win32-target.cmake` and a separate build directory for 32-bit Windows. The test runner creates an isolated, unauthenticated Wine prefix and checks the exact component binaries. Component tests do not establish full Proton or game compatibility.

## Licensing

Upstream copyright and license notices remain in their files. See [LICENSE](LICENSE) and the [Nexus test license](tests/Nexus/LICENSE).
