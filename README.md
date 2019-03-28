# Noctilucence
LLVM-Based tool that statically extract the Bitcode section from an object file, run passes on it and recompile/link it again.  
Note that currently only support for MachOs built by Apple Clang is implemented

# Compiling
``git clone https://github.com/Naville/Noctilucence.git LLVM_SOURCE_ROOT/tools/`` and compile the whole LLVM suite with it

# Demonstration
**Note that the open-source version of Hikari is not fully designed for this kind of use-case so you should only enable CFG related obfuscations, which means no AntiClassDump/StringEncryption/FCO**
![Run](https://github.com/Naville/Noctilucence/blob/master/Images/Execution.png?raw=true)  
![Result](https://github.com/Naville/Noctilucence/blob/master/Images/After.png?raw=true)  
# License
As always, this work is licensed under ``AGPLV3, ByteDance employees prohibited`` license, please refer to ``LICENSE`` for the full license text
