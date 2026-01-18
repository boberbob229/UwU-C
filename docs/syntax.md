# UwU-C Language Syntax Documentation

UwU-C is a systems programming language designed as a learning project, featuring a cute syntax inspired by internet culture, while maintaining C++ and Rust-like features for low-level control and memory safety. This document provides a simple guide to the core syntax elements of the UwU-C language.

## 1. Program Structure and Functions

The structure of a UwU-C program is similar to C, with a mandatory entry point function.

### Main Function

The program execution begins with the `main` function, which is declared using the keyword `nuzzle` (similar to `fn` in Rust or function in other languages). The return type is specified after the `->` operator.

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `nuzzle` | Function declaration keyword. |
| `->` | Separator for function return type. |
| `chonk` | The default integer type (likely a 32-bit integer). |

**Syntax:**

```uwu
nuzzle main() -> chonk {
    // Program logic goes here
    gimme 0; // Return statement
}
```

### Function Declaration and Return

Functions are declared with `nuzzle`, followed by the function name, parameters in parentheses, the return type, and the function body enclosed in curly braces `{}`. The `gimme` keyword is used to return a value from a function.

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `gimme` | The return statement (similar to `return`). |

**Example:**

```uwu
nuzzle add_two_numbers(a: chonk, b: chonk) -> chonk {
    gimme a + b;
}
```

## 2. Data Types and Variables

UwU-C replaces standard C-like type names with cute alternatives. Variables are declared with a name, followed by a colon `:`, the type, and an optional assignment using the `=` operator.

### Primitive Types

The following table maps the UwU-C primitive types to their likely C-style equivalents:

| UwU-C Type | Description | C-Style Equivalent (Inferred) |
| :--- | :--- | :--- |
| `boop` | Boolean type. | `bool` |
| `byte` | Smallest integer type. | `uint8_t` (unsigned 8-bit integer) |
| `smol` | Small integer type. | `int16_t` (16-bit integer) |
| `chonk` | Standard integer type. | `int32_t` (32-bit integer) |
| `megachonk` | Large integer type. | `int64_t` (64-bit integer) |
| `floof` | Single-precision floating-point. | `float` |
| `bigfloof` | Double-precision floating-point. | `double` |
| `void` | No value (used for functions that don't return). | `void` |

### Variable Declaration

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `const` | Declares a constant variable. |

**Syntax:**

```uwu
// Mutable variable declaration
my_variable: chonk = 10;

// Constant declaration
const PI: floof = 3.14159;
```

## 3. Control Flow

Control flow structures in UwU-C use keywords that correspond to standard programming constructs.

### Conditional Statements (If/Else)

Conditional logic uses `pwease` for the `if` block and `nowu` for the optional `else` block.

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `pwease` | The conditional `if` statement. |
| `nowu` | The optional `else` statement. |

**Syntax:**

```uwu
pwease (condition) {
    // Code if condition is true
} nowu {
    // Code if condition is false
}
```

### Loops (While and For)

UwU-C supports both `while` and `for` loops using the keywords `wepeat` and `fow`.

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `wepeat` | The `while` loop construct. |
| `fow` | The `for` loop construct. |
| `bweak` | Exits the current loop (similar to `break`). |
| `continyue` | Skips the rest of the current loop iteration (similar to `continue`). |

**Syntax (While Loop):**

```uwu
wepeat (condition) {
    // Loop body
}
```

**Syntax (For Loop):**

```uwu
fow (init_statement; condition; increment_statement) {
    // Loop body
}
```

## 4. Input/Output (I/O)

The primary function for outputting text to the console is `uwu_print`.

| UwU-C Function | Description |
| :--- | :--- |
| `uwu_print(string)` | Prints a string literal to the standard output. |

**Example:**

```uwu
uwu_print("Hello World!");
```

## 5. Other Keywords

UwU-C includes other keywords for more advanced systems programming features:

| UwU-C Keyword | Meaning |
| :--- | :--- |
| `stwuct` | Structure definition (similar to `struct`). |
| `enum` | Enumeration definition (similar to `enum`). |
| `smoosh` | Union definition (similar to `union`). |
| `unsafe` | Marks a block of code as unsafe (similar to Rust's `unsafe`). |
| `static` | Declares a static variable or function. |
| `extern` | Declares an external function or variable. |
| `typedef` | Creates an alias for a type. |
| `sizeof` | Returns the size of a type or variable. |
| `nuww` | Represents a null pointer. |
| `true` | Boolean literal for true. |
| `false` | Boolean literal for false. |



