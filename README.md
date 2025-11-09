# CppGameDX11 — A Multiplayer 3D Sandbox Game with DirectX 11

**CppGameDX11** is a lightweight, open-world 3D sandbox game built in C++ using **DirectX 11**. Inspired by voxel-based building and parkour mechanics, it supports both **single-player creative mode** and **real-time multiplayer** over LAN.

## 🔧 Features

- **First- and third-person camera** with smooth movement and collision detection
- **Physics-based character controller** with ground detection, jumping, gravity, and air control
- **Creative building system**: place/remove blocks (grass, stone, wood, metal, brick, dirt, water, lava, etc.)
- **Procedural parkour course** with dynamic obstacles and balls
- **Multiplayer support**:
  - Host or join a game via `host` / `connect <IP>`
  - Real-time position & block sync
  - Built-in console for commands (`help`, `fly`, `speed`, `gravity`, `load`, `save`, etc.)
- **Dynamic sky & lighting presets**: 20+ environments (sunset, storm, arctic, neon, mars, etc.)
- **Customizable FOV, jump sound volume, fly mode, and more**
- **Inventory system** with hotbar and block selection
- **Console with command history** and network debugging tools (`netdebug`, `ip`, `players`)

## 🎮 Controls

- **WASD** — Move  
- **Space** — Jump  
- **Shift** — Run  
- **Left Click** — Break block  
- **Right Click** — Place block  
- **Tab** — Toggle inventory (Build mode only)  
- **Tilde (~)** — Open console  
- **F** — Toggle fly mode  
- **V** — Toggle third-person view  


## 📦 Dependencies

- Windows 10/11
- Visual Studio (C++ workload)
- DirectX 11 SDK (built into Windows SDK)
- `iphlpapi.lib` (for network interface detection)
