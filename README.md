# VolumetricCloud

This is an experimental ray marching volumetric cloud code prototype using DirectX11 aiming integration into a flight simulator (FalconBMS).

![image](https://github.com/user-attachments/assets/0fb7f3ad-c4c6-462c-944e-9df307e24812)

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

## Clone

```bash
git clone --recurse-submodules https://github.com/chihirobelmo/volumetric-cloud-for-directx11.git
```

or if you forgot --recurse-submodules

```bash
git submodule add https://github.com/ocornut/imgui.git imgui
git submodule update --init --recursive
git submodule update --remote imgui
```
