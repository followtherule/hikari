This is my small renderer for learning Vulkan and graphics programming.

### Current Features

- free flying camera controller
- dual mode renderer (graphics pipeline and raytracing pipeline) that can be toggled online
- skybox
- glTF model loading

### Build Requirements

- [CMake](https://cmake.org/)
- gcc compiler that supports C++20
- [vulkansdk](https://vulkan.lunarg.com/sdk/home)
- gpu driver that supports Vulkan 1.4
- OpenCL headers and OpenCL driver required by [KTX-Software](https://github.com/KhronosGroup/KTX-Software)

### Build & Run

At the root of the repo:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./bin/Release/hikari_app
```
