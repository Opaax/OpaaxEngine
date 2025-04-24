# Opaax Game Engine

A game engine built from scratch in **C++20**, designed for modularity and future 3D support.  
Powered by **Vulkan** for rendering, **Jolt Physics** for collision and dynamics, and **SDL** for platform abstraction. Includes an ImGui-powered debug UI and modern memory management with custom allocators.

---

## Features

- **Custom Engine Architecture**  
  Inspired by *Game Engine Architecture* by Jason Gregory. Clean, layered design with strict separation of concerns.

- **Memory Management System**  
  Includes Stack, Pool, and Free List allocators, with integrated memory tracking and stack trace logging.

- **Rendering with Vulkan**  
  Low-level rendering backend using Vulkan for maximum control and performance.

- **Physics Integration**  
  Real-time 2D physics using Jolt, with plans for 3D support in future iterations.

- **Debug UI with ImGui**  
  In-engine tools for inspecting entities, performance, and memory in real time.

- **Input & Platform Abstraction**  
  Cross-platform support powered by SDL for input, windowing, and system events.

---

## Roadmap

- [ ] Core architecture (WIP)
- [ ] Vulkan-based renderer (WIP)
- [ ] Memory system
- [ ] Jolt Physics integration
- [ ] Entity-component system
- [ ] Scene serialization & tools
- [ ] Scripting (C++ or external, TBD)

---

## Philosophy

Built with clarity and extensibility in mind.  
Codebase prioritizes readability, modular design, and educational value—ideal for learning how game engines work under the hood.

---

## Dependencies

- [Vulkan SDK](https://vulkan.lunarg.com/)
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
- [SDL3](https://www.libsdl.org/)
- [ImGui](https://github.com/ocornut/imgui)