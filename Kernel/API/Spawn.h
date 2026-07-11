/*
 * Copyright (c) 2026, Fırat Kızılboğa <firatkizilboga11@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Traits.h>
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

}

namespace AK {

template<>
struct Traits<Kernel::SpawnFileActionHeader> : public DefaultTraits<Kernel::SpawnFileActionHeader> {
    static constexpr bool is_trivially_serializable() { return true; }
};

template<>
struct Traits<Kernel::SpawnFileActionDup2> : public DefaultTraits<Kernel::SpawnFileActionDup2> {
    static constexpr bool is_trivially_serializable() { return true; }
};

template<>
struct Traits<Kernel::SpawnFileActionOpen> : public DefaultTraits<Kernel::SpawnFileActionOpen> {
    static constexpr bool is_trivially_serializable() { return true; }
};

template<>
struct Traits<Kernel::SpawnFileActionClose> : public DefaultTraits<Kernel::SpawnFileActionClose> {
    static constexpr bool is_trivially_serializable() { return true; }
};

template<>
struct Traits<Kernel::SpawnFileActionChdir> : public DefaultTraits<Kernel::SpawnFileActionChdir> {
    static constexpr bool is_trivially_serializable() { return true; }
};

template<>
struct Traits<Kernel::SpawnFileActionFchdir> : public DefaultTraits<Kernel::SpawnFileActionFchdir> {
    static constexpr bool is_trivially_serializable() { return true; }
};

}
