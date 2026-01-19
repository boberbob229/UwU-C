# UwU-C 

UwU-C is a small programming language and compiler I'm building as a learning project. 

The goal is to explore how compilers actually work—from parsing and IR to handwritten assembly—while experimenting with safer memory ideas and a cute syntax. 

> [!WARNING]
> **This is not a production language.** It's a playground for learning low-level internals.

## Join the Community 

Got questions, want to follow the progress, or just want to hang out? Join the Discord:

**[Join the UwU-C Discord](https://discord.gg/brUugmXhux)**

## Why UwU-C? 

I wanted to stop treating compilers like a black box. UwU-C exists to help me learn:

*   **Compiler internals** — The journey from source to binary.
*   **Stack layout & calling conventions** — Managing memory at the metal.
*   **x86_64 & ARM64 assembly** — Writing instructions by hand.
*   **ABI rules** — And the fun of seeing why breaking them causes crashes.

Instead of using LLVM, UwU-C generates assembly directly. This makes the behavior much easier to understand, debug, and trace.

## Features 

*   **Custom IR** — My own intermediate representation.
*   **Handwritten codegen** — Raw assembly output for full control.
*   **Experimental memory safety** — Testing out some safer memory ideas.
*   **Cute syntax** — Making low-level dev a bit more fun and cute.
*   **Work in progress** — Always evolving.

## Project Status 

**Early and experimental.**  
Things will break. That’s part of the point.

## Supported Platforms 

| Architecture | OS |
| :--- | :--- |
| **x86_64** | Linux |
| **ARM64** | Linux, macOS |

## Building 

UwU-C supports two build systems: **Make** (simple and straightforward) or **Bazel** (fast with great caching).

### Using Make

```bash
# Build release version
make

# Build debug version (with AddressSanitizer)
make debug

# Build and run tests
make test

# Build examples
make examples

# Clean build artifacts
make clean
```

### Using Bazel

```bash
# Build the compiler
bazel build //:uwucc

# Build in debug mode
bazel build --config=debug //:uwucc

# Run the compiler
bazel run //:uwucc -- input.uwu -o output

# Compile an example (e.g., example/ez.uwu)
bazel build //:example_ez
bazel run //:example_ez

# Clean
bazel clean
```

For more details on Bazel usage, see [toasty-software-guide-to-bazel.md](docs/toasty-software-guide-to-bazel.md).

### Requirements

- C compiler (clang recommended, gcc works too)
- Make or Bazel/Bazelisk
- Platform: Linux or macOS
- Architecture: x86_64 or ARM64

## Repo Layout 

*   `src/` — compiler source
*   `include/` — headers
*   `stdlib/` — experimental standard library
*   `example/` — example programs
*   `docs/` — notes and explanations

## License 

**WTFPL** — do whatever you want.
