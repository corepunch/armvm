# armvm

**armvm** is a project that provides an ARMv7 compiler (from assembly) and a virtual machine capable of running ARM32 code on any architecture. This tool takes ARM assembly files compiled by Xcode (using specific flags for ARM32) and compiles them into a virtual machine that exposes external functionality through slots. One such example is the `printf` function, which the user must provide and pass into the VM.

## Features

- **ARMv7 Compiler**: Compile ARM assembly code (ARM32) using Xcode.
- **ARM32 Virtual Machine**: Emulate ARM32 code execution on any host architecture.
- **External Functionality**: The VM exposes slots for user-provided functions like `printf` and potentially others.
- **Cross-Architecture Support**: Run ARM32 code on non-ARM architectures without the need for hardware support.

## Requirements

- **Xcode**: For compiling ARM assembly files using specific flags for ARM32.
- **Custom Functions**: The user must provide implementations for any required external functions (e.g., `printf`).
- **Host Architecture**: Should be able to run on any platform (macOS, Linux, Windows, etc.) that supports the virtual machine.

## Usage

### 1. Compile ARM Assembly Code
Ensure that your ARM assembly file is compiled using Xcode with the appropriate flags for ARM32.  
Example flags in Xcode might look like: `-arch armv7 -m32` for ARM32 compilation.

### 2. Load and Run on the Virtual Machine
Load the compiled ARM32 code into the `armvm` virtual machine.  
Provide any external functions (like `printf`) required by the ARM code when loading the VM.

### 3. Provide External Functions
External functions such as `printf` need to be passed to the virtual machine at runtime.  
Example in C (for `printf`):

```c
void custom_printf(const char *str) {
    printf("%s", str);  // Implement as needed
}
```
Pass custom_printf into the VM when initializing.
Run the VM:
The virtual machine will then execute the ARM32 code in an emulated environment, providing the necessary hooks for the external functionality.
Example Code

Here is an example of how to set up the VM and pass in custom functions:

```
#include "armvm.h"

// User-defined printf implementation
void custom_printf(const char *str) {
    printf("%s", str);  // Implement as needed
}

int main() {
    // Initialize the VM
    armvm_t vm;
    armvm_init(&vm);

    // Load compiled ARM32 code into the VM
    armvm_load(&vm, "compiled_arm32_code.bin");

    // Register external functions (e.g., printf)
    armvm_register_function(&vm, "printf", custom_printf);

    // Run the ARM32 code on the VM
    armvm_run(&vm);

    // Cleanup
    armvm_cleanup(&vm);
    
    return 0;
}
```
#API
```
armvm_init(armvm_t *vm)
Initialize the ARM virtual machine.

armvm_load(armvm_t *vm, const char *file)
Load the compiled ARM32 binary into the virtual machine.

armvm_register_function(armvm_t *vm, const char *name, void *func)
Register an external function (e.g., printf) with the virtual machine.

armvm_run(armvm_t *vm)
Execute the ARM32 code within the virtual machine.

armvm_cleanup(armvm_t *vm)
Cleanup the virtual machine resources after execution.
```
License

This project is licensed under the MIT License. See the LICENSE file for more details.
