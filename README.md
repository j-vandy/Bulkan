# Bulkan
User-friendly real-time physically based 3D rendering library serving as a high-level wrapper for the Vulkan graphics API

## Windows Development Environment
- Download [Visual Studio Code](https://code.visualstudio.com/download).
- Download [MSYS2](https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe).
- In the wizard, choose your desired Installation Folder. Record this directory for later.
- Install MinGW-w64 toolchain with the following command:
```
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
```
- In the Windows search bar, type *Settings* to open your Windows Settings.
- Search for *Edit environment variables for your account*.
- In your *User variables*, select the *Path* variable and then select *Edit*.
- Select *New* and add the MinGW-w64 destination folder you recorded. If you used the default settings, then this will be the path: C:\msys64\ucrt64\bin.
- Select *OK*, and then select *OK* again in the *Environment Variables* window.
- To check that MinGW-w64 tools correctly installed, open a new Command Prompt and type:
```
gcc --version
g++ --version
gdb --version
```
- Download [CMake Windows x64 Installer](https://cmake.org/download/).
- To check that CMake correctly installed, open a new Command Prompt and type:
```
cmake --version
```
-  