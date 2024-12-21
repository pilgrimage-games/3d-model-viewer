# 3D Model Viewer

3D Model Viewer renders 3D models using a custom game engine developed by
Pilgrimage Games. It serves as a basic demonstration of the 3D capabilities of
the proprietary engine.

![screenshot](https://github.com/pilgrimage-games/3d-model-viewer/blob/main/screenshot.png?raw=true)

All code has been written from scratch in-house following "handmade"
principles, with minimal use of external libraries and other dependencies. The
only exception to this is [Dear ImGui](https://github.com/ocornut/imgui) for
the GUI. In lieu of even the C standard library, Pilgrimage Games maintains its
own internal common library.

While 3D Model Viewer is open-source, the underlying game engine and its
various graphics backends, as well as internal library and platform code,
remain closed-source. See
[Releases](https://github.com/pilgrimage-games/3d-model-viewer/releases) to
download and run the application.

## Features
* Supports various graphics APIs:
    * Direct3D 11
    * Direct3D 12
    * OpenGL (may experience driver issues on Intel graphics)
    * Vulkan
* Physically Based Rendering (PBR) with support for albedo, metallic-roughness,
  normal, and emissive textures
* Normal mapping
* Transparency and translucency
* Arcball camera (camera is rotated on a sphere around the model)
* Mouse/keyboard and gamepad controls for model selection, rotation, zoom, etc.
* UI for displaying model names, performance metrics, etc.
* Ultrawide support
* Bindless rendering (D3D12/Vulkan only)

## Models
The included 3D models are processed from their original glTF 2.0 binary format
(.glb) and packed into a single Pilgrimage Games Assets (.pga) file.

### Attributions
* "Baker and the Bridge" by Conrad Justin is licensed under [Sketchfab Standard](https://help.sketchfab.com/hc/en-us/articles/115004276346-Licenses#standard).
* "Corset" by Microsoft is Public Domain.
* "Damaged Helmet" by theblueturtle_ is licensed under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/).
* "Ftm" by luyssport is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).
* "Metal-Rough Spheres" by Ed Mackey is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
* "PlayStation 1" by Lars Maes is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
* "Ship in a Bottle" by Loic Norgeot is licensed under [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/).
* "Water Bottle" by Microsoft is Public Domain.

