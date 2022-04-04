# Asuna
Asuna is a renderer base on vulkan ray tracing pipeline with NVIDIA card.

## Dependencies

+ C++ compiler. `MSVC 19.29.30141.0` was tested.
+ [CMake](https://cmake.org/download/). `Version 3.23` was tested.
+ [NVIDIA driver](https://www.nvidia.com/Download/index.aspx). Should be released on or after December 15th, 2020, for the implementations of the new official versions of the Vulkan ray tracing extensions. `RTX 3060 Ti` with Driver `Version 472.12` was tested. *Whether this project supports non NVIDIA graphics cards has not been determined.*
+ [Vulkan SDK](https://vulkan.lunarg.com/). Should be greater than `Version 1.3.204.0`. `Version 1.3.204.1` was tested.

## Build

```bash
$ git clone git@github.com:f1shel/Asuna.git --recursive
$ mkdir Asuna/build
$ cd Asuna/build
$ cmake ..
$ make (or build in IDE)
```

The binary file is placed in the `Asuna/bin_x64` or `Asuna/bin_x86` folder.

## Documentation

+ **Initializing**: window, surface, context, device, allocator

+ **Parsing scene**: interpret scene file and generate meta data

+ **Allocating scene resources**: convert meta data to gpu resources

+ **Building pipeline**

+ **Binding pipeline and resources**

+ **Rendering** 

```c++
class AsunaTracer : public AppBaseVk {
	class Context; ----------------
    ----- class Scene;--------- | |
    | --- class Resources; <--|-| |
    |-|-> class Pipeline;  <------|
    void run();
};
```

  

