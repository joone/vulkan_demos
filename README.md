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

