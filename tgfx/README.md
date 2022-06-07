# TGFX

TGFX is a lightweight 2D graphics library which provides high-performance APIs that work across a
variety of hardware platforms. The main goal of TGFX is to achieve the best balance between binary 
size and performance by taking advantage of available platform APIs as many as possible. It serves
as the graphics engine for the PAG library and other libraries.

TGFX is under active development and the APIs are subject to change.

### Platform support via backing renderers

| Vector Backend |  GPU Backend   |      Target Platforms        |    Status     |
|:--------------:|:--------------:|:----------------------------:|:-------------:|
|    FreeType    |  OpenGL        |  All                         |   complete    |
|  CoreGraphics  |  OpenGL        |  iOS, macOS                  |   complete    |
|    Canvas2D    |  WebGL         |  Web                         |   complete    |
|  CoreGraphics  |  Metal         |  iOS, macOS                  |  in progress  |
|    FreeType    |  Vulkan        |  Android, Linux              |    planned    |

