# About

An open-source reimplementation of the ARM11-side AM (**A**pplication **M**anager) system module for the Nintendo 3DS.
This is my first extensive reverse-engineer-and-reimplement project, it's been a while since I started and now it's finally done.

All of the AM-specific code has been written based on the stock implementation with some differences:
- Thread stacks are using memory in the `.data` section instead of being dynamically allocated (along with some other data being in `.data` instead of being heap-allocated)
- The CIA installation code supports misaligned writes and installs contents in batches of 64 or less depending on the amount of contents.
- Content import write sizes are now limited exactly to what AM9 supports.

I have decided not to give specific names to IPC commands because it would cause a massive amount of differences between what is known on sites like 3DBrew and the actual functionality of each command.
Instead, I decided to give each command a small description that actually matches the behavior of the command.

# Compiling

Requirements:
- Latest version of devkitARM
- makerom on PATH

The following environment variables can be set to alter the output:
- `DEBUG`: if this is set, all optimization is disabled and debug symbols are included in the output ELF.
- `REPLACE_AM`: if this is set, the output CXI will use the stock title ID, which allows you to install it in place of the stock version.
- `DEBUG_PRINTS`: if this is set, you will see debug messages when attaching to the process in GDB.

To compile:
- `[optional settings] make`

For example:
`DEBUG=1 REPLACE_AM=1 make` would create a build that would replace the stock version and would also include debug symbols.

# Licensing

The project itself is under the GPLv3 license.

Parts of [libctru](https://github.com/devkitPro/libctru) were taken and modified (function argument order, names, etc.). The license for libctru can be seen here: [LICENSE.libctru.md](/LICENSE.libctru.md)

The licensing terms for the allocator can be found in the source file for it: [allocator.c](/source/allocator.c).

The licensing terms for the SHA-256 implementation can be found here: [LICENSE.sha256.md](/LICENSE.sha256.md).

# Special thanks
[@luigoalma](https://github.com/luigoalma): quite literally teaching me how to reverse engineer
