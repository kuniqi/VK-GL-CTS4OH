Vulkan CTS README
=================

This document describes how to build and run Vulkan Conformance Test suite.

Vulkan CTS is built on the dEQP framework.
Up-to-date documentation for dEQP is available at:

* [The VK-GL-CTS wiki for Khronos members](https://gitlab.khronos.org/Tracker/vk-gl-cts/wikis/home)
* [The VK-GL-CTS wiki for non-Khronos members](https://github.com/KhronosGroup/VK-GL-CTS/wiki)


Requirements
------------

### Common

The following tools must be installed and present in the PATH variable:

 * Git (for checking out sources)
 * Python 3.x (for the build related scripts, some other scripts still use Python 2.7.x)
 * CMake 3.20.0 or newer

### Win32

 * Visual Studio 2015 or newer (glslang uses several C++11 features)

### Linux

 * Standard toolchain (make, gcc/clang)
 * If you have X11 installed, then the build assumes you also have the `GL/glx.h` header
   file.
    * You can get this from the `mesa-common-dev` Ubuntu package.

### MacOS

 * Xcode

Vulkan SDK for macOS includes a MoltenVK runtime library which is recommended method for
developing a Vulkan application.

### Android

 * Android NDK r17c or later.
 * Android SDK with: SDK Tools, SDK Platform-tools, SDK Build-tools, and API 28
 * Java Development Kit (JDK)
 * Windows: either NMake or Ninja in PATH

If you have downloaded the Android SDK command line tools package (25.2.3 or higher) then
you can install the necessary components by running:

	tools/bin/sdkmanager tools platform-tools 'build-tools;25.0.2' 'platforms;android-28'


Building CTS
------------

To build dEQP, you need first to download sources for zlib, libpng, jsoncpp, glslang,
vulkan-docs, spirv-headers, and spirv-tools.

To download sources, run:

	python3 external/fetch_sources.py

You may need to re-run `fetch_sources.py` to update to the latest glslang,
vulkan-docs and spirv-tools revisions occasionally.

You also need to install lxml python module by running:

	python3 -m pip install lxml

With CMake out-of-source builds are always recommended. Create a build directory
of your choosing, and in that directory generate Makefiles or IDE project
using cmake.

If you intend to run the Vulkan CTS video decode or encode tests, you must first
download the required sample clips by running the two helper scripts in the
`external/` directory:

	python3 external/fetch_video_decode_samples.py
	python3 external/fetch_video_encode_samples.py

Each script will pull down the necessary video files into the CTS data tree.
Both scripts support the `--help` flag to list all available options.

### Windows x86-32

	cmake <path to vulkancts> -G"Visual Studio 14"
	start dEQP-Core-default.sln

### Windows x86-64

	cmake <path to vulkancts> -G"Visual Studio 14 Win64"
	start dEQP-Core-default.sln

### Linux 32-bit Debug

	cmake <path to vulkancts> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32
	make -j

Release build can be done by using -DCMAKE_BUILD_TYPE=Release

If building for 32-bit x86 with GCC, you probably also want to add `-msse2
-mfpmath=sse` to ensure that you get correct IEEE floating-point behavior.

### Linux 64-bit Debug

	cmake <path to vulkancts> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64
	make -j


### MacOS 64-bit Debug

	cmake <path to vulkancts> -DCMAKE_BUILD_TYPE=Debug -DDEQP_TARGET=osx -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64
	make -j

### Android

There's two types of builds for Android:

#### App

This builds an APK that needs to be invoked via `adb shell` and the output needs
to be read via `adb logcat`, it's the preferred way for long-running invocations
on Android since it doesn't depend on an active connection from the host PC.

Following command will build dEQP.apk:

	python3 scripts/android/build_apk.py --sdk <path to Android SDK> --ndk <path to Android NDK>

By default the CTS package will be built for the Android API level 28.
Another API level may be supplied using --native-api command line option.

The package can be installed by either running:

	python3 scripts/android/install_apk.py

By default the CTS package will contain libdeqp.so built for armeabi-v7a, arm64-v8a,
x86, and x86_64 ABIs, but that can be changed at build time by passing the --abis command line
option to `scripts/android/build_apk.py`.

To pick which ABI to use at _install time_, use the following command instead:

	adb install -g --abi <ABI name> <build-root>/package/dEQP.apk

#### Executable

This is identical to the builds on other platforms and is better for iterative
runs of headless tests as CTS can be invoked and the output can be checked from
a single interactive terminal.

This build doesn't support WSI tests and shouldn't be used for conformance
submissions, it also isn't recommended for longer running tests since Android
will terminate this process as soon as the `adb shell` session ends which may
happen due to an unintentional device disconnection.

	cmake <path to vulkancts> -GNinja -DCMAKE_BUILD_TYPE=Debug \
	      -DCMAKE_TOOLCHAIN_FILE=<NDK path>/build/cmake/android.toolchain.cmake \
	      -DCMAKE_ANDROID_NDK=<NDK path> -DANDROID_ABI=<ABI to build eg: arm64-v8a> \
	      -DDE_ANDROID_API=<API level> -DDEQP_TARGET_TOOLCHAIN=ndk-modern \
	      -DDEQP_TARGET=android -DDEQP_ANDROID_EXE=ON
	ninja all

The build needs to be transferred to the device via `adb push` to a directory
under `/data/` on the device, such as `/data/local/tmp/` which should be writeable
for non-rooted devices. It should be noted that anywhere on `/sdcard/` won't work
since it's mounted as `noexec`.

### Note on Debug Build Link Times

Some CTS binaries like `deqp-vk` and `deqp-vksc` are notably large when built
with debug information. For example, as of the time this text is being written,
`deqp-vk` is over 700 MiB big. As a consequence of this and the number of
symbols, linking these binaries takes a long time on some environments. For
example, on Linux with the BFD or Gold linkers, which are still the default in
many distributions, linking these binaries in debug mode may take over 10
seconds even on a relatively fast CPU, and many more if the CPU is slow.

Typically on Linux, both the LLD linker from the LLVM project and the Mold
linker are able to link these binaries much faster, in less than a second on the
same fast CPU, using varying amounts of memory and threads.

On both Ubuntu and Fedora, the LLD linker is provided by the "lld" package and
the Mold linker is provided by the "mold" package. Once installed, there are
several ways to use them when building CTS.

#### Using lld or mold as the default system-wide linkers

Under both Ubuntu and Fedora, `update-alternatives` can be used to set the
default link for the `ld` tool and make it point to `ld.lld` or `ld.mold`
instead of `ld.bfd` or `ld.gold`. Once set, the linker will be used by default
when linking any binary.

#### Using lld or mold only when building CTS

Both GCC and Clang can be told to use a different linker with the
`-fuse-ld=LINKER` command-line option at link time. For example, `-fuse-ld=lld`
or `-fuse-ld=mold`. CMake will automatically pick up that option and use it when
set in the `LDFLAGS` environment variable before configuring the project, so the
following example should work:

```
LDFLAGS="-fuse-ld=lld ${LDFLAGS}"
export LDFLAGS
<cmake configuration command>
```

Note in this case the linker name is passed without the `ld.` prefix.

Building Mustpass
-----------------

Current Vulkan mustpass is checked into repository and can be found at:

	external/vulkancts/mustpass/main/vk-default.txt

Current Vulkan SC mustpass can be found at:

	external/vulkancts/mustpass/main/vksc-default.txt

This file contains list of files located in vk-default or vksc-default subdirectory. Those files contain
tests from bigest test groups and together they contain all test cases that should pass.

Vulkan CTS mustpass can be re-generated by running:

	python3 <vulkancts>/external/vulkancts/scripts/build_mustpass.py


Running CTS
-----------

Following command line options MUST be used when running CTS:

	--deqp-caselist-file=<vulkancts>/external/vulkancts/mustpass/main/vk-default.txt (or vksc-default.txt for Vulkan SC implementations)
	--deqp-log-images=disable
	--deqp-log-shader-sources=disable

If an implementation ships with [implicit layers](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/loader/LoaderAndLayerInterface.md#implicit-vs-explicit-layers) enabled, then such layers must be enabled
when running CTS.

In addition, on multi-device systems the device for which conformance is claimed
can be selected with:

	--deqp-vk-device-id=<value>

To speed up the conformance run on some platforms the following command line
option may be used to disable frequent fflush() calls to the output logs:

	--deqp-log-flush=disable

By default, the test log will be written into the path "TestResults.qpa". If the
platform requires a different path, it can be specified with:

	--deqp-log-filename=<path>

By default, the CTS will expect to find its test resource files in the current
working directory. This can be overridden with:

	--deqp-archive-dir=<path>

By default, the shader cache will be written into the path "shadercache.bin". If the
platform requires a different path, it can be specified with:

	--deqp-shadercache-filename=<path>

If the shader cache is not desired, it can be disabled with:

	--deqp-shadercache=disable

CTS execution may be split into N fractions ( for the purpose of running it in parallel ) using

	--deqp-fraction=I,N

where I denotes index of current CTS execution ( I=[0..N-1], N=[1..16] )

When collecting results for a Conformance Submission Package the number of fractions must not exceed 16,
and a list of mandatory information tests for each fraction must be supplied:

	--deqp-fraction-mandatory-caselist-file=<vulkancts>external/vulkancts/mustpass/main/vk-fraction-mandatory-tests.txt

To specify file containing waived tests that are omitted only by specified vendor and renderer/device
the following command line option may be used:

	--deqp-waiver-file=<path>

To make log more clear for high level shader source analysis
output of decompiled SPIR-V shader sources can be disabled:

	--deqp-log-decompiled-spirv=disable

Also non-informative empty LogInfo sections can be removed
from output into log by specifying:

	--deqp-log-empty-loginfo=disable

Vulkan compute-only implementations must be tested using option

	--deqp-compute-only=enable

There are several additional options used only in conjunction with Vulkan SC tests
( for Vulkan SC CTS tests deqp-vksc application should be used ).

To define minimum size of the command pool use parameter:

	--deqp-command-pool-min-size=<value>

To define minimum size of a single command buffer use parameter:

	--deqp-command-buffer-min-size=<value>

If minimum size will not be defined, then the size of single command buffer will be estimated
from number of commands multiplied by value of parameter deqp-command-default-size.
Default size of a single command may be defined using:

	--deqp-command-default-size=<value>

Another group of Vulkan SC options enable use of offline pipeline compiler:

	--deqp-pipeline-compiler=<value>
	--deqp-pipeline-dir=<value>
	--deqp-pipeline-args=<value>
	--deqp-pipeline-file=<value>
	--deqp-pipeline-logfile=<value>
	--deqp-pipeline-prefix=<value>

In case of offline pipeline compilers the size of the pipeline will be returned by
pipeline compiler. If we use internal pipeline compilation then pipeline size will be
equal to default size. This size may be modified using:

	--deqp-pipeline-default-size=<value>

Vulkan SC may be implemented on embedded platform that is not able to
read from/write to files, write logs, etc. In this case you may use
external server that will perform these tasks on another machine:

	--deqp-server-address=<value>

In Vulkan SC CTS tests are performed twice. You may define how many tests
are performed on separate process in a single batch:

	--deqp-subprocess-test-count=<value>

Above mentioned parameter works as default value for all tests.
You may also define how many tests are performed on a separate process
for specific test tree branch using parameter:

	--deqp-subprocess-cfg-file=<path>

File should contain test pattern followed by numeric value, for example:
dEQP-VKSC.api.copy_and_blit.core.image_to_image.dimensions.src32768x4_dst32768x4.*, 5
dEQP-VKSC.texture.explicit_lod.2d.sizes.*, 20
dEQP-VKSC.texture.explicit_lod.2d.sizes.128x128_*, 4

There is also one option used by CTS internally and should not be used manually.
It informs deqp-vksc application that it works as subprocess:

	--deqp-subprocess=[enable|disable]

For platforms where it is needed to override the default loader library path, this option can be used (e.g. loader library vulkan-1.dll):

	--deqp-vk-library-path=<path>

No other command line options are allowed.

### Win32

	Vulkan:

	cd <builddir>/external/vulkancts/modules/vulkan
	Debug\deqp-vk.exe --deqp-caselist-file=...

	Vulkan SC:

	cd <builddir>/external/vulkancts/modules/vulkan
	Debug\deqp-vksc.exe --deqp-caselist-file=...

Test log will be written into TestResults.qpa

### Linux

	Vulkan:

	cd <builddir>/external/vulkancts/modules/vulkan
	./deqp-vk --deqp-caselist-file=...

	Vulkan SC:

	cd <builddir>/external/vulkancts/modules/vulkan
	./deqp-vksc --deqp-caselist-file=...

### MacOS

	cd <builddir>/external/vulkancts/modules/vulkan
	./deqp-vk --deqp-caselist-file=...

### Android

#### App

For Android build using SDK 29 or greater, it is recommended to use `/sdcard/Documents/` instead of `/sdcard/` due to scoped storage.

	adb push <vulkancts>/external/vulkancts/mustpass/main/vk-default.txt /sdcard/vk-default.txt
	adb shell

In device shell:

	am start -n com.drawelements.deqp/android.app.NativeActivity -e cmdLine "deqp --deqp-caselist-file=/sdcard/vk-default.txt --deqp-log-images=disable --deqp-log-shader-sources=disable --deqp-log-filename=/sdcard/TestResults.qpa"

Test progress will be written to device log and can be displayed with:

	adb logcat -s dEQP

Test log will be written into `/sdcard/TestResults.qpa`.

#### Executable

Identical to [Linux](#linux-1), but within `adb shell` instead:

	adb shell
	> cd <pushed build directory>/external/vulkancts/modules/vulkan
	> ./deqp-vk --deqp-caselist-file=...

Conformance Submission Package Requirements
-------------------------------------------

The conformance submission package must contain the following:

1. Full test logs (`TestResults.qpa`) from CTS runs against all driver builds and all fractions
2. Result of `git status` and `git log` from CTS source directory
3. Any patches used on top of release tag
4. Conformance statement

Test logs (1) should be named `<submission pkg dir>/TestResults-<driver build type>-<fraction id>-of-<total fractions>.qpa`,
for example `TestResults-armeabi-v7a-1-of-8.qpa`. On platforms where multiple different driver
builds (for example 64-bit and 32-bit) are present, CTS logs must be provided
for each driver build as part of the submission package. If CTS run was split into multiple
fractions then result files for all fractions must be provided, each file must
contain results of the mandatory information tests.

Fractions may be run on different physical devices but each device must represent
the same Conformant Product.

Test logs generated on a system which exposes more than one physical device
in a device group can be used for products that expose one or more physical
devices in their device group.

The CTS build must always be done from clean git repository that doesn't have any
uncommitted changes. Thus it is necessary to run and capture output of `git
status` and `git log` (2) in the source directory:

	git status > <submission pkg dir>/git-status.txt
	git log --first-parent <release tag>^..HEAD > <submission pkg dir>/git-log.txt

Any changes made to CTS must be committed to the local repository, and provided
as part of the submission package (3). This can be done by running:

	git format-patch -o <submission pkg dir> <release tag>..HEAD

Changes to platform-specific code (mostly under `framework/platform`)
are allowed. The commit message for each change must include a clear
description of the change and why it is necessary.
For Vulkan SC, changes are also permitted to the following:
- vksc-pipeline-compiler (under `vkscpc/`)
- vksc-server (under `vkscserver/`)
- modules/vulkan/sc/vktApplicationParametersTests.cpp (to provide vendor-specific test data)

Bugfixes to the tests are allowed. Before being used for a submission,
bugfixes must be accepted and merged into the CTS repository.
`git cherry-pick` is strongly recommended as a method of applying bug fixes.

If command line parameter --deqp-subprocess-cfg-file was used then the file
pointed by this parameter must also be added to submission package.

Other changes must be accompanied by a waiver (see below).

NOTE: When cherry-picking patches on top of release tag, please use `git cherry-pick -x`
to include original commit hash in the commit message.

Conformance statement (4) must be included in a file called `STATEMENT-<adopter>`
and must contain following:

	CONFORM_VERSION:         <git tag of CTS release>
	PRODUCT:                 <string-value>
	CPU:                     <string-value>
	OS:                      <string-value>

Note that product/cpu/os information is also captured in `dEQP-VK.info.*` tests
if `vk::Platform::describePlatform()` is implemented.

If the submission package covers multiple products, you can list them by appending
additional `PRODUCT:` lines to the conformance statement. For example:

	CONFORM_VERSION:         vulkan-cts-1.2.6.0
	PRODUCT:                 Product A
	PRODUCT:                 Product B
	...

The actual submission package consists of the above set of files which must
be bundled into a gzipped tar file.

For Vulkan this must be named `VK<API major><API minor>_<adopter><_info>.tgz`.

For Vulkan SC this must be named `VKSC<API major><API minor>_<adopter><_info>.tgz`.

`<API major>` is the major version of the Vulkan {SC} API specification.
`<API minor>`is the minor version of the Vulkan {SC} API specification.

`<adopter>` is the name of the Adopting member company, or some recognizable abbreviation.
The `<_info>` field is optional. It may be used to uniquely identify a submission
by OS, platform, date, or other criteria when making multiple submissions.
For example, a company XYZ may make a submission for a Vulkan 1.1 implementation named
`VK11_XYZ_PRODUCTA_Windows10.tgz`

One way to create a suiteable gzipped tar file is to execute the command:

	tar -cvzf <filename.tgz> -C <submission pkg dir> .

where `<submission pkg dir>` is the directory containing the files from (1)-(4)
from above. A submission package must contain all of the files listed above,
and only those files.

As an example submission package could contain:

	STATEMENT-Khronos
	git-log.txt
	git-status.txt
	0001-Remove-Waived-Filtering-Tests.patch
	0002-Fix-Pipeline-Parameters.patch
	TestResults-armeabi-v7a.qpa
	TestResults-arm64-v8a.qpa


Waivers
-------

The process for requesting a waiver is to report the issue by filing a bug
report in the Gitlab VulkanCTS project (TODO Github?). When creating the
submission package, include references to the waiver in the commit message of
the relevant change. Including as much information as possible in your bug
report (including a unified diff or a merge request of suggested file changes)
will ensure the issue can be progressed as rapidly as possible. Issues must
be labeled "Waiver" (TODO!) and identify the version of the CTS and affected
tests.

Conformance Criteria
--------------------

Conformance run is considered passing if all tests finish with allowed result
codes. Test results are contained in the TestResults.qpa log. Each
test case section contains XML tag Result, for example:

	<Result StatusCode="Pass">Not validated</Result>

The result code is the value of the StatusCode attribute. Following status
codes are allowed:

	Pass
	NotSupported
	QualityWarning
	CompatibilityWarning
	Waiver

Submission package can be verified using `verify_submission.py`
script located in [VK-GL-CTS-Tools](https://github.com/KhronosGroup/VK-GL-CTS-Tools).

Vulkan platform port
--------------------

Vulkan support from Platform implementation requires providing
`getVulkanPlatform()` method in `tcu::Platform` class implementation.

See `framework/common/tcuPlatform.hpp` and examples in
`framework/platform/win32/tcuWin32Platform.cpp` and
`framework/platform/android/tcuAndroidPlatform.cpp`.

If any WSI extensions are supported, platform port must also implement
methods for creating native display (`vk::Platform::createWsiDisplay`)
and window handles (`vk::wsi::Display::createWindow`). Otherwise tests
under `dEQP-VK.wsi` will fail.


Null (dummy) driver
-------------------

For testing and development purposes it might be useful to be able to run
tests on dummy Vulkan implementation. One such implementation is provided in
vkNullDriver.cpp. To use that, implement `vk::Platform::createLibrary()` with
`vk::createNullDriver()`.


Validation Layers
-----------------

Vulkan CTS framework includes first-party support for validation layers, that
can be turned on with `--deqp-validation=enable` command line option.

When validation is turned on, default instance and device will be created with
validation layers enabled and debug callback is registered to record any
messages. Debug messages collected during test execution will be included at
the end of the test case log.

In addition, when the `--deqp-print-validation-errors` command line option is
used, validation errors are additionally printed to standard error in the
moment they are generated.

If any validation errors are found, test result will be set to `InternalError`.

By default `VK_DEBUG_REPORT_INFORMATION_BIT_EXT` and `_DEBUG_BIT_EXT` messages
are excluded from the log, but that can be customized by modifying
`vk::DebugReportMessage::shouldBeLogged()` in `vkDebugReportUtil.hpp`.

On the Android target, layers can be added to the APK during the build process
by setting the `--layers-path` command line option to point to the downloaded
Validation Layers binaries or a locally-built layers tree. The layers are
expected to be found under $abi/ under the layers path.
The Validation Layers releases including prebuilt binaries are available at
https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases.


Cherry GUI
----------

Vulkan test module can be used with Cherry (GUI for test execution and
analysis). Cherry is available at
https://android.googlesource.com/platform/external/cherry. Please follow
instructions in README to get started.

Before first launch, and every time test hierarchy has been modified, test
case list must be refreshed by running:

	python scripts/build_caselists.py path/to/cherry/data

Cherry must be restarted for the case list update to take effect.


Shader Optimizer
----------------

Vulkan CTS can be optionally run with the shader optimizer enabled. This
is an experimental feature which can be used to further stress both the
drivers as well as the optimizer itself. The shader optimizer is disabled
by default.

The following command line options can be used to configure the shader
optimizer:

	--deqp-optimization-recipe=<number>

The list of the optimization recipes can be found and customized in the
`optimizeCompiledBinary()` function in `vkPrograms.cpp`.

As of this writing, there are 2 recipes to choose from:

	0. Disabled (default)
	1. Optimize for performance
	2. Optimize for size

The performance and size optimization recipes are defined by the spir-v
optimizer, and will change from time to time as the optimizer matures.

	--deqp-optimize-spirv=enable

This option is not required to run the optimizer. By default, the shader
optimizer only optimizes shaders generated from GLSL or HLSL, and leaves
hand-written SPIR-V shaders alone.

Many of the hand-written SPIR-V tests stress specific features of the
SPIR-V which might get optimized out. Using this option will enable the
optimizer on the hand-written SPIR-V as well, which may be useful in
finding new bugs in drivers or the optimizer itself, but will likely
invalidate the tests themselves.


Shader Cache
------------

The Vulkan CTS framework contains a shader cache for speeding up the running
of the CTS. Skipping shader compilation can significantly reduce runtime,
especially for repeated runs.

Default behavior is to have the shader cache enabled, but truncated at the
start of the CTS run. This still gives the benefit of skipping shader
compilation for identical shaders in different tests (which there are many),
while making sure that the shader cache file does not grow indefinitely.

The shader cache identifies the shaders by hashing the shader source code
along with various bits of information that may affect the shader compilation
(such as shader stage, CTS version, possible compilation flags, etc). If a
cached shader with matching hash is found, a byte-by-byte comparison of the
shader sources is made to make sure that the correct shader is being
retrieved from the cache.

The behavior of the shader cache can be modified with the following command
line options:

	--deqp-shadercache=disable

Disable the shader cache. All shaders will be compiled every time.

	--deqp-shadercache-filename=<filename>

Set the name of the file where the cached shaders will be stored. This
option may be required for the shader cache to work at all on Android
targets.

	--deqp-shadercache-truncate=disable

Do not truncate the shader cache file at startup. No shader compilation will
occur on repeated runs of the CTS.

	--deqp-shadercache-ipc=enable

Enables the use of inter-process communication primitives to allow several
instances of CTS to share a single cache file. All of the instances must
use the same shader cache filename.

Note that if one instance should crash while holding the cache file lock,
the other instances will hang. The lock is only held while reading or
writing to the cache, so crashes are unlikely.

In case of a crash outside the cache file lock, the named shared memory
and shared semaphore may be left behind. These will be re-used by CTS on
subsequent runs, so additional memory leak will not occur. Shader cache
truncate may not work in this case. On Windows, when all instances of
CTS have terminated the shared resources get automatically cleaned up.

RenderDoc
---------
The RenderDoc (https://renderdoc.org/) graphics debugger may be used to debug
Vulkan tests.

Following command line option should be used when launching tests from
RenderDoc UI:

	--deqp-renderdoc=enable

This causes the framework to interface with the debugger and mark each dEQP
test case as a separate 'frame', just for the purpose of capturing. The frames
are added using RenderDoc 'In-Application API', instead of swapchain operations.

Third Party Runners
-------------------

Some CTS tests use third-party runners. By default all tests are executed
regardless of runner type (`any`). To exclude all tests using any of the
external runners (`none`) or to only include tests using a certain runner:

	--deqp-runner-type=(any|none|amber)

Vulkan SC Conformance Test suite
--------------------------------

This project is also able to perform conformance tests for Vulkan SC
implementations. For this purpose Vulkan CTS framework has been adapted
to Vulkan SC requirements:

- Vulkan SC CTS contains its own mustpass list

  external/vulkancts/mustpass/main/vksc-default.txt

- Vulkan SC CTS uses its own executable module to perform tests: deqp-vksc

- Each test in deqp-vksc is performed twice.
  First test run is performed in main process and its purpose is to collect
  information about used pipelines, number of created Vulkan objects etc.
  Second test run is done in separate process ( subprocess ) and it performs
  the real tests.

- Vulkan SC pipelines may be compiled using offline pipeline compiler
  delivered by implementation vendor. You can use command line parameters
  to achieve this ( see parameters: --deqp-pipeline-compiler, --deqp-pipeline-dir,
  --deqp-pipeline-args, --deqp-pipeline-file, --deqp-pipeline-logfile,
  --deqp-pipeline-prefix )

  Reference offline pipeline compiler was created to showcase how input and output
  should look like for such application. It uses Vulkan API to create pipeline cache.
  The name of the executable is vksc-pipeline-compiler.

- Some of the future Vulkan SC implementations may not have a possibility to use
  filesystem, create pipeline caches or log results to file. For these implementations
  Vulkan SC CTS contains server application that may handle such requests on external
  host machine. Define parameter --deqp-server-address in deqp-vksc application
  to use external server.
  Server application's name is vksc-server and its parameters are listed below,
  in Command Line section.

Command Line
------------
Full list of parameters for the `deqp-vk` and `deqp-vksc` modules:

OpenGL and OpenCL parameters not affecting Vulkan API were suppressed.

  -h, --help
    Show this help

  -q, --quiet
    Suppress messages to standard output

  -n, --deqp-case=<value>
    Test case(s) to run, supports wildcards (e.g. dEQP-GLES2.info.*)

  --deqp-caselist=<value>
    Case list to run in trie format (e.g. {dEQP-GLES2{info{version,renderer}}})

  --deqp-caselist-file=<value>
    Read case list (in trie format) from given file

  --deqp-caselist-resource=<value>
    Read case list (in trie format) from given file located application's assets

  --deqp-stdin-caselist
    Read case list (in trie format) from stdin

  --deqp-log-filename=<value>
    Write test results to given file
    default: 'TestResults.qpa'

  --deqp-runmode=[execute|xml-caselist|txt-caselist|stdout-caselist]
    Execute tests, or write list of test cases into a file
    default: 'execute'

  --deqp-caselist-export-file=<value>
    Set the target file name pattern for caselist export
    default: '${packageName}-cases.${typeExtension}'

  --deqp-watchdog=[enable|disable]
    Enable test watchdog
    default: 'disable'

  --deqp-crashhandler=[enable|disable]
    Enable crash handling
    default: 'disable'

  --deqp-base-seed=<value>
    Base seed for test cases that use randomization
    default: '0'

  --deqp-test-iteration-count=<value>
    Iteration count for cases that support variable number of iterations
    default: '0'

  --deqp-visibility=[windowed|fullscreen|hidden]
    Default test window visibility
    default: 'windowed'

  --deqp-surface-width=<value>
    Use given surface width if possible
    default: '-1'

  --deqp-surface-height=<value>
    Use given surface height if possible
    default: '-1'

  --deqp-surface-type=[window|pixmap|pbuffer|fbo]
    Use given surface type
    default: 'window'

  --deqp-screen-rotation=[unspecified|0|90|180|270]
    Screen rotation for platforms that support it
    default: '0'

  --deqp-vk-device-id=<value>
    Vulkan device ID (IDs start from 1)
    default: '1'

  --deqp-vk-device-group-id=<value>
    Vulkan device Group ID (IDs start from 1)
    default: '1'

  --deqp-log-images=[enable|disable]
    When disabled, prevent any image from being logged into the test results file
    default: 'enable'

  --deqp-log-all-images=[enable|disable]
    When enabled, log all images from image comparison routines as if COMPARE_LOG_EVERYTHING was used in the code
    default: 'disable'

  --deqp-log-shader-sources=[enable|disable]
    Enable or disable logging of shader sources
    default: 'enable'

  --deqp-test-oom=[enable|disable]
    Run tests that exhaust memory on purpose
    default: 'enable'

  --deqp-archive-dir=<value>
    Path to test resource files
    default: '.'

  --deqp-log-flush=[enable|disable]
    Enable or disable log file fflush
    default: 'enable'

  --deqp-log-compact=[enable|disable]
    Enable or disable the compact version of the log
    default: 'disable'

  --deqp-validation=[enable|disable]
    Enable or disable test case validation
    default: 'disable'

  --deqp-spirv-validation=[enable|disable]
    Enable or disable spir-v shader validation
    default: 'disable' in release builds, 'enable' in debug builds

  --deqp-print-validation-errors
    Print validation errors to standard error

  --deqp-duplicate-case-name-check=[enable|disable]
    Check for duplicate case names when creating test hierarchy
    default: 'enable' in Debug mode, 'disable' in Release mode

  --deqp-optimization-recipe=<value>
    Shader optimization recipe (0=disabled, 1=performance, 2=size)
    default: '0'

  --deqp-optimize-spirv=[enable|disable]
    Apply optimization to spir-v shaders as well
    default: 'disable'

  --deqp-shadercache=[enable|disable]
    Enable or disable shader cache
    default: 'enable'

  --deqp-shadercache-filename=<value>
    Write shader cache to given file
    default: 'shadercache.bin'

  --deqp-shadercache-truncate=[enable|disable]
    Truncate shader cache before running tests
    default: 'enable'

  --deqp-renderdoc=[enable|disable]
    Enable RenderDoc frame markers
    default: 'disable'

  --deqp-fraction=<value>
    Run a fraction of the test cases (e.g. N,M means run group%M==N)
    default: ''

  --deqp-fraction-mandatory-caselist-file=<value>
    Case list file that must be run for each fraction
    default: ''

  --deqp-waiver-file=<value>
    Read waived tests from given file
    default: ''

  --deqp-runner-type=[any|none|amber]
    Filter test cases based on runner
    default: 'any'

  --deqp-terminate-on-fail=[enable|disable]
    Terminate the run on first failure
    default: 'disable'

  --deqp-terminate-on-device-lost=[enable|disable]
    Terminate the run on first device lost error
    default: 'enable'

  --deqp-compute-only=[enable|disable]
    Perform tests for devices implementing compute-only functionality
    default: 'disable'

  --deqp-subprocess=[enable|disable]
    Inform app that it works as subprocess (Vulkan SC only, do not use manually)
    default: 'disable'

  --deqp-subprocess-test-count=<value>
    Define default number of tests performed in subprocess (Vulkan SC only)
    default: '65536'

  --deqp-subprocess-cfg-file=<path>
	Config file defining number of tests performed in subprocess for specific test branches (Vulkan SC only)
    default: ''

  --deqp-server-address=<value>
    Server address (host:port) responsible for shader compilation (Vulkan SC only)
    default: ''

  --deqp-command-pool-min-size=<value>
    Define minimum size of the command pool (in bytes) to use (Vulkan SC only)
	default: '0'

  --deqp-command-buffer-min-size=<value>
    Define minimum size of the command buffer (in bytes) to use (Vulkan SC only)
	default: '0'

  --deqp-app-params-input-file=<path>
    File providing default application parameters (Vulkan SC only)
    default: ''

    The file should contain lines of input in the following format:
    type ("instance" or "device"), 32-bit vendorID, 32-bit deviceID, 32-bit parameterKey, 64-bit parameterValue

    `type` specifies whether the values will be used for instance or device creation.
    All the other values should be encoded in hex. For example:
    instance, 0x01234567, 0x76543210, 0x01234567, 0x0000000076543210

  --deqp-command-default-size=<value>
    Define default single command size (in bytes) to use (Vulkan SC only)
	default: '256'

  --deqp-pipeline-default-size=<value>
    Define default pipeline size (in bytes) to use (Vulkan SC only)
	default: '16384'

  --deqp-pipeline-compiler=<value>
    Path to offline pipeline compiler (Vulkan SC only)
    default: ''

  --deqp-pipeline-dir=<value>
    Offline pipeline data directory (Vulkan SC only)
    default: ''

  --deqp-pipeline-args=<value>
    Additional compiler parameters (Vulkan SC only)
    default: ''

  --deqp-pipeline-file=<value>
    Output file with pipeline cache (Vulkan SC only, do not use manually)
    default: ''

  --deqp-pipeline-logfile=<value>
    Log file for pipeline compiler (Vulkan SC only, do not use manually)
    default: ''

  --deqp-pipeline-prefix=<value>
    Prefix for input pipeline compiler files (Vulkan SC only, do not use manually)
    default: ''

Full list of parameters for the `vksc-server` application:

  --port
    Port
    default: '59333'

  --log
    Log filename
    default: 'dummy.log'

  --pipeline-compiler
    Path to offline pipeline compiler
    default: ''

  --pipeline-dir
    Offline pipeline data directory
    default: ''

  --pipeline-file
    Output file with pipeline cache
    default: ''

  --pipeline-log
    Compiler log file
    default: 'compiler.log'

  --pipeline-args
    Additional compiler parameters
    default: ''
