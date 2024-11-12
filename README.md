# VolumetricCloud

This is an experimental ray marching volumetric cloud code using DirectX11 for integration into a flight simulator.

![image](https://github.com/user-attachments/assets/58330575-8980-48f0-b120-600b29a96e71)

# Goals

- Achieve a speed of 1-2ms (500fps or higher), considering that in the actual game we may render it twice for VR
- Perform depth buffer checks with polygon renderings
- Support a large area cloud map, such as a 1024 x 1024 km theater
- Provide large area cloud visibility, ideally 15-30 nautical miles beyond
- Offer a variety of cloud types that morph and change with the weather

# Getting Started

## Install DirectX SDK

Download and Intall:
https://www.microsoft.com/en-us/download/details.aspx?id=6812

## Clone and include imgui

Clone the ImGui repository and set it up by following their documentation. My recommendation is to copy their code into your source directory and adjust the include paths as needed.

https://github.com/ocornut/imgui

Undefine `#define USE_IMGUI` if you don't want to use ImGui.

## Visual Studio Settings

1.	Open Project Properties:
    1.  Right-click on your project in the Solution Explorer and select "Properties".
2.	Add Additional Include Directories:
    1.  Go to Configuration `Properties -> VC++ Directories -> Include Directories`.
    2.  Add the path to the DirectX SDK include directory, e.g., `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include`.
3.	Add Additional Library Directories:
    1.  Go to Configuration `Properties -> VC++ Directories -> Library Directories`.
    2.  Add the path to the DirectX SDK library directory, e.g., `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x64`.
4.	Add Additional Dependencies:
    1.  Go to Configuration `Properties -> Linker -> Input -> Additional Dependencies`.
    2.  Add `d3d11.lib;D3DCompiler.lib;` to the list.

If adding an HLSL shader file to your project causes an "entry point not found" error, it suggests that the issue might be related to how the shader file is being compiled or linked within the project. Here are some steps to troubleshoot and resolve this issue:

Steps to Resolve the Issue
1.	Ensure Shader Compilation is Separate: Make sure that the HLSL shader file is not being treated as a C++ source file. It should be compiled separately using the DirectX shader compiler.
2.	Exclude Shader from Build: Ensure that the shader file is excluded from the C++ build process.
3.	Correctly Compile Shaders: Use the appropriate DirectX functions to compile the shaders at runtime.

Detailed Steps
1. Ensure Shader Compilation is Separate
    HLSL shader files should not be compiled as part of the C++ build process. They should be compiled using the DirectX shader compiler (e.g., D3DCompileFromFile).
2. Exclude Shader from Build
    1.	Right-click on the Shader File:
        1.	In the Solution Explorer, right-click on the shader file (e.g., shader.hlsl).
    2.	Properties:
        1.	Select "Properties" from the context menu.
    3.	Exclude from Build:
        1.	In the Properties window, set "Item Type" to "Does Not Participate in Build".
