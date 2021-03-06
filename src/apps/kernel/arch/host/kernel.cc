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

#include <m3/Log.h>
#include <m3/Config.h>
#include <m3/util/SList.h>
#include <m3/DTU.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <dirent.h>

#include "../../KWorkLoop.h"
#include "../../Services.h"
#include "../../PEManager.h"
#include "../../KVPE.h"
#include "../../SyscallHandler.h"
#include "dev/TimerDevice.h"
#include "dev/VGAConsole.h"

using namespace m3;

static SList<Device> devices;
static size_t fssize = 0;

class KernelEPSwitcher : public EPSwitcher {
public:
    virtual void switch_ep(size_t victim, capsel_t, capsel_t newcap) override {
        // we don't need to clear endpoint-registers since nobody does cmpxchg here.
        if(newcap != Cap::INVALID) {
            MsgCapability *c = static_cast<MsgCapability*>(
                CapTable::kernel_table().get(newcap, Capability::MSG));
            assert(c != nullptr);
            DTU::get().configure(victim, c->obj->label, c->obj->core, c->obj->epid, c->obj->credits);
            LOG(IPC, "Kernel programs SEP[" << victim << "] to "
                    << "core=" << c->obj->core << ", ep=" << c->obj->epid
                    << ", lbl=" << fmt(c->obj->label, "#0x", sizeof(label_t) * 2));
        }
    }
};

static void sigint(int) {
    WorkLoop::get().stop();
}

static void delete_dir(const char *dir) {
    char path[64];
    DIR *d = opendir(dir);
    struct dirent *e;
    while((e = readdir(d))) {
        if(strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        unlink(path);
    }
    closedir(d);
    rmdir(dir);
}

static void copyfromfs(MainMemory &mem, const char *file) {
    int fd = open(file, O_RDONLY);
    if(fd < 0)
        PANIC("Opening '" << file << "' for reading failed");

    ssize_t res = read(fd, (void*)mem.addr(), mem.size());
    if(res == -1)
        PANIC("Reading from '" << file << "' failed");
    close(fd);

    // remove that from the available memory
    mem.map().allocate(res);

    fssize = res;
    LOG(DEF, "Copied fs-image '" << file << "' to 0.." << fmt(fssize, "#x"));
}

static void copytofs(MainMemory &mem, const char *file) {
    char name[256];
    snprintf(name, sizeof(name), "%s.out", file);
    int fd = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if(fd < 0)
        PANIC("Opening '" << name << "' for writing failed");
    write(fd, (void*)mem.addr(), fssize);
    close(fd);

    LOG(DEF, "Copied fs-image from memory back to '" << name << "'");
}

// overwrite weak-symbol __default_conf. this creates the Config object for the kernel
int __default_conf = 0;

int main(int argc, char *argv[]) {
    const char *fsimg = nullptr;
    mkdir("/tmp/m3", 0755);
    KernelEPSwitcher *epsw = new KernelEPSwitcher();
    EPMux::get().set_epswitcher(epsw);
    signal(SIGINT, sigint);

    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "-c") == 0)
            devices.append(new VGAConsoleDevice());
        else if(strcmp(argv[i], "-t") == 0)
            devices.append(new TimerDevice());
        else if(strncmp(argv[i], "fs=", 3) == 0)
            fsimg = argv[i] + 3;
    }

    int argstart = 0;
    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "--") == 0) {
            argstart = i;
            break;
        }
    }

    if(fsimg)
        copyfromfs(MainMemory::get(), fsimg);
    LOG(DEF, "Initializing PEs.");
    PEManager::create(argc - argstart - 1, argv + argstart + 1);
    KWorkLoop::run();

    LOG(DEF, "Shutting down.");
    if(fsimg)
        copytofs(MainMemory::get(), fsimg);
    PEManager::destroy();
    for(auto it = devices.begin(); it != devices.end(); ) {
        auto old = it++;
        old->stop();
        delete &*old;
    }
    delete epsw;
    delete_dir("/tmp/m3");
    return EXIT_SUCCESS;
}
