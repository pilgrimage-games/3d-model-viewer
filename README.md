# 3D Model Viewer

3D Model Viewer renders 3D models using a custom game engine developed by
Pilgrimage Games. It serves as a basic demonstration of the 3D capabilities of
the proprietary engine.

![screenshot](https://github.com/pilgrimage-games/3d-model-viewer/blob/main/screenshot.png?raw=true)

All code was written from scratch in-house following "handmade" principles,
with minimal use of external libraries and other dependencies. In lieu of the C
standard library, Pilgrimage Games maintains its own internal common library.
The only exceptions to this are for reading specific file formats
([stb_image](https://github.com/nothings/stb) and
[cgltf](https://github.com/jkuhlmann/cgltf)) and for creating the UI ([Dear
ImGui](https://github.com/ocornut/imgui)).

While 3D Model Viewer is open-source, the underlying game engine and its
various graphics backends, as well as internal library and platform code,
remain closed-source. See
[Releases](https://github.com/pilgrimage-games/3d-model-viewer/releases) to
download and run the application.

## Features
* Supports various graphics APIs:
    * Direct3D 11
    * Direct3D 12
    * OpenGL (NOTE: Possible issues with Intel integrated graphics)
    * Vulkan
* Physically Based Rendering (PBR) with support for albedo, metallic-roughness,
  normal, and emissive textures
* UI controls to live-switch graphics APIs, rotate the model, change the light
  direction, etc.
* Renders any user-provided 3D models located in `assets/models`, provided they
  meet the following requirements:
    * glTF 2.0 binary format (.glb)
    * Unsigned 16-bit indices
    * Vertex tangent data

