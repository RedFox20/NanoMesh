#pragma once
#include <rpp/debugging.h>

#define NanoErr(opt, message, ...) \
        if ((opt) & Nano::Options::NoThrow) { LogError(message, ##__VA_ARGS__); return false; } \
        else { ThrowErrType(Nano::MeshIOError, message, ##__VA_ARGS__); }

