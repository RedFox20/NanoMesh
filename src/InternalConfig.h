#pragma once
#include <rpp/debugging.h>

#define NanoErr(opt, message, ...) \
        if ((opt).NoExceptions) { LogError(message, ##__VA_ARGS__); } \
        else { ThrowErrType(Nano::MeshIOError, message, ##__VA_ARGS__); }

