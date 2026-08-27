
> [!WARNING]
> ## This project is deprecated and will be archived
> 

⚠️This project has been archived and is no longer maintained.
We does not provide security support, vulnerability review, patches, releases, or CVE assignment/coordination for this project. Vulnerability reports related to this project may be declined on the grounds that the project is unsupported.
Users should carefully evaluate the security implications before using this project.
However, a third-party CNA or the CVE Program may still assign a CVE ID under its own policies.
For questions regarding such assignments, please contact the CVE Program directly.



# rlottie

[![Build Status](https://travis-ci.org/Samsung/rlottie.svg?branch=master)](https://travis-ci.org/Samsung/rlottie)
[![Build status](https://ci.appveyor.com/api/projects/status/n3xonxk1ooo6s7nr?svg=true&passingText=windows%20-%20passing)](https://ci.appveyor.com/project/smohantty/rlottie-mliua)
[![Gitter](https://badges.gitter.im/rLottie-dev/community.svg)](https://gitter.im/rLottie-dev/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

<p align="center">
  <img width="240" height="240" src="https://github.com/Samsung/rlottie/blob/master/.Gifs/logo.png">
</p>

rlottie is a platform independent standalone c++ library for rendering vector based animations and art in realtime.

Lottie loads and renders animations and vectors exported in the bodymovin JSON format. Bodymovin JSON can be created and exported from After Effects with [bodymovin](https://github.com/bodymovin/bodymovin), Sketch with [Lottie Sketch Export](https://github.com/buba447/Lottie-Sketch-Export), and from [Haiku](https://www.haiku.ai).

For the first time, designers can create and ship beautiful animations without an engineer painstakingly recreating it by hand. Since the animation is backed by JSON they are extremely small in size but can be large in complexity!

Here are small samples of the power of Lottie.

<p align="center">
  <img src="https://github.com/Samsung/rlottie/blob/master/.Gifs/1.gif">
  <img src="https://github.com/Samsung/rlottie/blob/master/.Gifs/2.gif">
  <img src="https://github.com/airbnb/lottie-ios/blob/master/_Gifs/Examples1.gif">
</p>

## Contents
- [Building Lottie](#building-lottie)
	- [Meson Build](#meson-build)
	- [Cmake Build](#cmake-build)
	- [Test](#test)
- [Demo](#demo)
- [Previewing Lottie JSON Files](#previewing-lottie-json-files)
- [Quick Start](#quick-start)
- [Dynamic Property](#dynamic-property)
- [Supported After Effects Features](#supported-after-effects-features)
- [Issues or Feature Requests?](#issues-or-feature-requests)

## Building Lottie
rlottie supports [meson](https://mesonbuild.com/) and [cmake](https://cmake.org/) build system. rlottie is written in `C++14`. and has a public header dependency of `C++11`

### Meson Build
install [meson](http://mesonbuild.com/Getting-meson.html) and [ninja](https://ninja-build.org/) if not already installed.

Run meson to configure rlottie
```
meson build
```
Run ninja to build rlottie
```
ninja -C build
```

### Cmake Build

Install [cmake](https://cmake.org/download/) if not already installed

Create a build directory for out of source `build`
```
mkdir build
```
Run cmake command inside `build` directory to configure rlottie.
```
cd build
cmake ..

# install in a different path. eg ~/test/usr/lib
cmake -DCMAKE_INSTALL_PREFIX=~/test ..

# static build
cmake -DBUILD_SHARED_LIBS=OFF ..
```
Run make to build rlottie

```
make -j 2
```
To install rlottie library

```
make install
```

### Test

Configure to build test
```
meson configure -Dtest=true
```
Build test suit

```
ninja
```
Run test suit
```
ninja test
```
[Back to contents](#contents)

#
## Demo
If you want to see rlottie library in action without building it please visit [rlottie online viewer](http://rlottie.com)

While building rlottie library it generates a simple lottie to GIF converter which can be used to convert lottie json file to GIF file.

Run Demo
```
lottie2gif [lottie file name]
```

#
## Previewing Lottie JSON Files
Please visit [rlottie online viewer](http://rlottie.com)

[rlottie online viewer](http://rlottie.com) uses rlottie wasm library to render the resource locally in your browser. To test your JSON resource drag and drop it to the browser window.

#
## Quick Start

Lottie loads and renders animations and vectors exported in the bodymovin JSON format. Bodymovin JSON can be created and exported from After Effects with [bodymovin](https://github.com/bodymovin/bodymovin), Sketch with [Lottie Sketch Export](https://github.com/buba447/Lottie-Sketch-Export), and from [Haiku](https://www.haiku.ai).

You can quickly load a Lottie animation with:
```cpp
auto animation = rlottie::Animation::loadFromFile("absolute_path/test.json");
```
You can load a lottie animation from raw data with:
```cpp
auto animation = rlottie::Animation::loadFromData(std::string(rawData), std::string(cacheKey));
```

Properties like `frameRate` , `totalFrame` , `duration` can be queried with:
```cpp
# get the frame rate of the resource.
double frameRate = animation->frameRate();

#get total frame that exists in the resource
size_t totalFrame = animation->totalFrame();

#get total animation duration in sec for the resource
double duration = animation->duration();
```
Render a particular frame in a surface buffer `immediately` with:
```cpp
rlottie::Surface surface(buffer, width , height , stride);
animation->renderSync(frameNo, surface);
```
Render a particular frame in a surface buffer `asyncronousely` with:
```cpp
rlottie::Surface surface(buffer, width , height , stride);
# give a render request
std::future<rlottie::Surface> handle = animation->render(frameNo, surface);
...
#when the render data is needed
rlottie::Surface surface = handle.get();
```

[Back to contents](#contents)


## Dynamic Property

You can update properties dynamically at runtime. This can be used for a variety of purposes such as:
- Theming (day and night or arbitrary themes).
- Responding to events such as an error or a success.
- Animating a single part of an animation in response to an event.
- Responding to view sizes or other values not known at design time.

### Understanding After Effects

To understand how to change animation properties in Lottie, you should first understand how animation properties are stored in Lottie. Animation properties are stored in a data tree that mimics the information hierarchy of After Effects. In After Effects a Composition is a collection of Layers that each have their own timelines. Layer objects have string names, and their contents can be an image, shape layers, fills, strokes, or just about anything that is drawable. Each object in After Effects has a name. Lottie can find these objects and properties by their name using a KeyPath.

### Usage

To update a property at runtime, you need 3 things:
1. KeyPath
2. rLottie::Property
3. setValue()

### KeyPath

A KeyPath is used to target a specific content or a set of contents that will be updated. A KeyPath is specified by a list of strings that correspond to the hierarchy of After Effects contents in the original animation.
KeyPaths can include the specific name of the contents or wildcards:
- Wildcard *
	- Wildcards match any single content name in its position in the keypath.
- Globstar **
	- Globstars match zero or more layers.

### Properties

`rLottie::Property` is an enumeration of properties that can be set. They correspond to the animatable value in After Effects and the available properties are listed below.
```cpp
enum class Property {
    FillColor,     /*!< Color property of Fill object , value type is rlottie::Color */
    FillOpacity,   /*!< Opacity property of Fill object , value type is float [ 0 .. 100] */
    StrokeColor,   /*!< Color property of Stroke object , value type is rlottie::Color */
    StrokeOpacity, /*!< Opacity property of Stroke object , value type is float [ 0 .. 100] */
    StrokeWidth,   /*!< stroke with property of Stroke object , value type is float */
    ...
};
```

### setValue()

`setValue()` requires a keypath of string and value. The value can be `Color`, `Size` and `Point` structure or a function that returns them. `Color`, `Size`, and `Point` vary depending on the type of `rLottie::Property`. This value or function(callback) is called and applied to every frame. This value can be set differently for each frame by using the `FrameInfo` argument passed to the function.


### Usage
```cpp
animation->setValue<rlottie::Property::FillColor>("**",rlottie::Color(0, 1, 0));
```

```cpp
animation->setValue<rlottie::Property::FillColor>("Layer1.Box 1.Fill1",
    [](const rlottie::FrameInfo& info) {
         if (info.curFrame() < 15 )
             return rlottie::Color(0, 1, 0);
         else {
             return rlottie::Color(1, 0, 0);
         }
     });
```

[Back to contents](#contents)

#
#
## Supported After Effects Features

| **Shapes** | **Supported** |
|:--|:-:|
| Shape | 👍 |
| Ellipse | 👍 |
| Rectangle | 👍 |
| Rounded Rectangle | 👍 |
| Polystar | 👍 |
| Group | 👍 |
| Trim Path (individually) | 👍 |
| Trim Path (simultaneously) | 👍 |
| **Renderable** | **Supported** |
| Fill  | 👍 |
| Stroke | 👍 |
| Radial Gradient | 👍 |
| Linear Gradient | 👍 |
| Gradient Stroke | 👍 |
| **Transforms** | **Supported** |
| Position | 👍 |
| Position (separated X/Y) | 👍 |
| Scale | 👍 |
| Skew | ⛔️ |
| Rotation | 👍 |
| Anchor Point | 👍 |
| Opacity | 👍 |
| Parenting | 👍 |
| Auto Orient | 👍 |
| **Interpolation** | **Supported** |
| Linear Interpolation | 👍 |
| Bezier Interpolation | 👍 |
| Hold Interpolation | 👍 |
| Spatial Bezier Interpolation | 👍 |
| Rove Across Time | 👍 |
| **Masks** | **Supported** |
| Mask Path | 👍 |
| Mask Opacity | 👍 |
| Add | 👍 |
| Subtract | 👍 |
| Intersect | 👍 |
| Lighten | ⛔️ |
| Darken | ⛔️ |
| Difference | ⛔️ |
| Expansion | ⛔️ |
| Feather | ⛔️ |
| **Mattes** | **Supported** |
| Alpha Matte | 👍 |
| Alpha Inverted Matte | 👍 |
| Luma Matte | 👍 |
| Luma Inverted Matte | 👍 |
| **Merge Paths** | **Supported** |
| Merge | ⛔️ |
| Add | ⛔️ |
| Subtract | ⛔️ |
| Intersect | ⛔️ |
| Exclude Intersection | ⛔️ |
| **Layer Effects** | **Supported** |
| Fill | ⛔️ |
| Stroke | ⛔️ |
| Tint | ⛔️ |
| Tritone | ⛔️ |
| Levels Individual Controls | ⛔️ |
| **Text** | **Supported** |
| Glyphs |  ⛔️ |
| Fonts | ⛔️ |
| Transform | ⛔️ |
| Fill | ⛔️ |
| Stroke | ⛔️ |
| Tracking | ⛔️ |
| Anchor point grouping | ⛔️ |
| Text Path | ⛔️ |
| Per-character 3D | ⛔️ |
| Range selector (Units) | ⛔️ |
| Range selector (Based on) | ⛔️ |
| Range selector (Amount) | ⛔️ |
| Range selector (Shape) | ⛔️ |
| Range selector (Ease High) | ⛔️ |
| Range selector (Ease Low)  | ⛔️ |
| Range selector (Randomize order) | ⛔️ |
| expression selector | ⛔️ |
| **Other** | **Supported** |
| Expressions | ⛔️ |
| Images | 👍 |
| Precomps | 👍 |
| Time Stretch |  👍 |
| Time remap |  👍 |
| Markers | 👍  |

#
[Back to contents](#contents)

## Issues or Feature Requests?
File github issues for anything that is broken. Be sure to check the [list of supported features](#supported-after-effects-features) before submitting.  If an animation is not working, please attach the After Effects file to your issue. Debugging without the original can be very difficult. For immidiate assistant or support please reach us in [Gitter](https://gitter.im/rLottie-dev/community#)
