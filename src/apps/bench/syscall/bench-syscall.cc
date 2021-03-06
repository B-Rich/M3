/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <m3/Common.h>
#include <m3/stream/Serial.h>
#include <m3/cap/MemGate.h>
#include <m3/util/Profile.h>
#include <m3/Syscalls.h>
#include <m3/DTU.h>
#include <m3/Log.h>

using namespace m3;

#define COUNT   100

int main() {
    cycles_t total = 0;
    for(int i = 0; i < COUNT; ++i) {
        cycles_t start = Profile::start(0);
        Syscalls::get().noop();
        cycles_t end = Profile::stop(0);
        total += end - start;
    }
    Serial::get() << "Per syscall: " << (total / COUNT) << "\n";
    return 0;
}
