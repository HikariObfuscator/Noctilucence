# Noctilucence
LLVM-Based tool that statically extract the Bitcode section from an object file, run passes on it and recompile/link it again.  
Note that currently only support for MachOs built by Apple Clang is implemented

# Why
Existing implementations suck because they do all the following which is plain retarded design in my opinion:
- Invoke a ton of processes through ``posix_spawn``, Noctilucence only invokes system's linker due to the lack of MachO support in LLD
- Instead of correctly handling linker flags, they tend to hard-encode linker flags

# Limitations
Noctilucence directly uses LLVM's CodeGen without invoking a ton of processes and it's doing so by implementing a minimum implementation stripped down from ``llc``, which means it could be less stable in cases. But then again this is just a toy project and serves as PoC purpose only.

# Compiling
``git clone https://github.com/Naville/Noctilucence.git LLVM_SOURCE_ROOT/tools/`` and compile the whole LLVM suite with it

# Demonstration
**Note that the open-source version of Hikari is not fully designed for this kind of use-case so you should only enable CFG related obfuscations, which means no AntiClassDump/StringEncryption/FCO**
![Run](https://github.com/Naville/Noctilucence/blob/master/Images/Execution.png?raw=true)  
![Result](https://github.com/Naville/Noctilucence/blob/master/Images/After.png?raw=true)  
# License
As always, this work is licensed under ``AGPLV3, ByteDance employees prohibited`` license, please refer to ``LICENSE`` for the full license text
