# Bulkan
Bulkan is a user-friendly real-time physically based 3D rendering library that serves as a high-level wrapper for the Vulkan graphics API. The project uses CMake as the build system and vcpkg as the package manager.

## Windows Development Environment Setup (Visual Studio Code)

### 1. Install and Configure MinGW-w64 Toolchain

- **Download MSYS2:**
  - Get the MSYS2 installer from [this link](https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe).
  - Run the installer and choose your desired installation folder. Make a note of this directory for future reference.

- **Install MinGW-w64 Toolchain:**
  - Open the MSYS2 terminal (either via the start menu or by navigating to the installation folder).
  - Run the following command to install the MinGW-w64 toolchain:
    ```
    pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
    ```

- **Add MinGW-w64 to the Path:**
  - Open **Windows Settings**.
  - Search for **Edit the system environment variables** and select it.
  - In the System Properties window, click on **Environment Variables**.
  - Under **User variables**, select the `Path` variable and click **Edit**.
  - Click **New** and add the MinGW-w64 `bin` folder path (e.g., `C:\msys64\ucrt64\bin` if you used the default installation directory).
  - Click **OK** to close all dialog boxes.

### 2. Install CMake

- **Download and Install CMake:**
  - Download the CMake Windows x64 Installer from [this link](https://cmake.org/download/).
  - Run the installer and follow the on-screen instructions to complete the installation.

### 3. Install and Configure vcpkg

- **Clone and Bootstrap vcpkg:**
  - Open a terminal (Command Prompt, PowerShell, or MSYS2 terminal).
  - Navigate to your desired installation directory and run the following commands:
     ```
     git clone https://github.com/microsoft/vcpkg.git
     cd vcpkg
     bootstrap-vcpkg.bat
     ```

- **Set Up `VCPKG_ROOT` Environment Variable:**
  - Open **Windows Settings**.
  - Search for **Edit the system environment variables** and select it.
  - In the System Properties window, click on **Environment Variables**.
  - Under **User variables**, click **New** to create a new environment variable.
  - For **Variable name**, enter `VCPKG_ROOT`.
  - For **Variable value**, enter the path to your vcpkg installation (e.g., `C:\path\to\vcpkg`).
  - Click **OK** to save the new variable.

- **Add vcpkg to the Path:**
  - Under **User variables**, select the `Path` variable and click **Edit**.
  - Click **New** and add `%VCPKG_ROOT%`.
  - Click **OK** to close all dialog boxes.
