/*
 * Copyright (c) 2026, Fırat Kızılboğa <firatkizilboga11@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/StdLibExtras.h>
#include <AK/Types.h>
#include <Kernel/API/POSIX/sys/types.h>

namespace Kernel {

enum class SpawnFileActionType : u8 {
    Dup2 = 0,
    Open = 1,
    Close = 2,
    Chdir = 3,
    Fchdir = 4,
};

struct SpawnFileActionHeader {
    SpawnFileActionType type;
    u16 record_length;
};

struct SpawnFileActionDup2 {
    SpawnFileActionHeader header;
    int old_fd;
    int new_fd;
};

struct SpawnFileActionOpen {
    SpawnFileActionHeader header;
    int fd;
    int flags;
    mode_t mode;
    u16 path_length;
};

struct SpawnFileActionClose {
    SpawnFileActionHeader header;
    int fd;
};

struct SpawnFileActionChdir {
    SpawnFileActionHeader header;
    u16 path_length;
};

struct SpawnFileActionFchdir {
    SpawnFileActionHeader header;
    int fd;
};

union SpawnFileActionUnion {
    SpawnFileActionHeader header;
    SpawnFileActionDup2 dup2;
    SpawnFileActionOpen open;
    SpawnFileActionClose close;
    SpawnFileActionChdir chdir;
    SpawnFileActionFchdir fchdir;
};

constexpr size_t SPAWN_FILE_ACTION_ALIGNMENT = alignof(SpawnFileActionUnion);

}
