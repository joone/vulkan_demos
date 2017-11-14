# Vulkan Demos

```
$ cd ~/git/chromium/src
$ git clone https://github.com/joone/vulkan_demos.git
$ gn args out/Release
```

Add the following configurations:

```
enable_vulkan = true
use_sysroot = false
import("//vulkan_demos/build/vulkan.gni")
```
```
$ ninja -C out/Release vulkan_test
```
Run vulkan_tests
```
$ out/Release/vulkan_test 
```

# Troubleshooting
## glslang build issue
See https://bugs.chromium.org/p/chromium/issues/detail?id=682523&desc=2#c15
