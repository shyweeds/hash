# Wish Shell 🐚

Wish Shell is a lightweight Unix-like shell implemented in C for fun!  
It demonstrates how a basic command-line interpreter works, including **parsing, process creation, execution, and I/O redirection**.

---

## 📌 Project Overview

Wish Shell is a simplified shell inspired by Unix shells such as `bash` and `sh`.

It supports:

- Command execution (`ls`, `echo`, `cat`, etc.)
- Built-in commands (e.g., `cd`, `exit`)
- Input/output redirection (only `>`)
- Background execution (`&`)
- Basic command parsing and tokenization
- Process creation using `fork()` and `exec()`

The goal of this project is to understand **Linux process model, system calls, and shell internals**.

---

## ⚙️ Architecture

Wish Shell follows a classic shell execution pipeline:

User Input → Lexer → Parser → Executor → OS (fork/exec)

### Key Components:
- **Tokenizer**: splits input into commands and arguments
- **Parser**: interprets redirection and background execution
- **Executor**: handles process creation and execution
- **Process Manager**: uses `wait()/waitpid()` for synchronization

---

## 🚀 Build & Run

### Compile
```bash
gcc -o wish wish.c -g -O0 -Wall
```

### Run
```bash
./wish
```
