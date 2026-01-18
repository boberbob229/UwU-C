# UwU-C ✨

UwU-C is a small programming language and compiler I'm building as a learning project. 

The goal is to explore how compilers actually work—from parsing and IR to handwritten assembly—while experimenting with safer memory ideas and a cute syntax. 

> [!WARNING]
> **This is not a production language.** It's a playground for learning low-level internals.

## Join the Community 💬

Got questions, want to follow the progress, or just want to hang out? Join the Discord:

**[Join the UwU-C Discord](https://discord.gg/brUugmXhux)**

## Why UwU-C? 🤔

I wanted to stop treating compilers like a black box. UwU-C exists to help me learn:

*   **Compiler internals** — The journey from source to binary.
*   **Stack layout & calling conventions** — Managing memory at the metal.
*   **x86_64 & ARM64 assembly** — Writing instructions by hand.
*   **ABI rules** — And the fun of seeing why breaking them causes crashes.

Instead of using LLVM, UwU-C generates assembly directly. This makes the behavior much easier to understand, debug, and trace.

## Features 🚀

*   **Custom IR** — My own intermediate representation.
*   **Handwritten codegen** — Raw assembly output for full control.
*   **Experimental memory safety** — Testing out some safer memory ideas.
*   **Cute syntax** — Making low-level dev a bit more fun and cute.
*   **Work in progress** — Always evolving.

## Project Status ⚠️

**Early and experimental.**  
Things will break. That’s part of the point.

## Supported Platforms 💻

| Architecture | OS |
| :--- | :--- |
| **x86_64** | Linux |
| **ARM64** | Linux, macOS |

## Repo Layout 📂

*   `src/` — compiler source
*   `include/` — headers
*   `stdlib/` — experimental standard library
*   `example/` — example programs
*   `docs/` — notes and explanations

## License ⚖️

**WTFPL** — do whatever you want.
