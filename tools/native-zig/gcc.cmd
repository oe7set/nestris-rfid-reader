@echo off
rem Forwards to zig (portable clang) for "pio test -e native" on Windows without gcc.
uvx --from ziglang python -m ziglang cc %*
