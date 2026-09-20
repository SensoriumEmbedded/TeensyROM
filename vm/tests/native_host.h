// SPDX-License-Identifier: MIT
//
// A complete base-profile VmHost backed by the real filesystem, for running
// extension modules on a development machine. It implements exactly what the
// loader implements -- same handle limits, same validation, same refusals -- so
// a module that passes here is exercising the contract, not a friendlier mock.
//
// This is deliberately part of the published ABI directory's tooling: build
// your module natively against this host to debug it, then cross-compile the
// identical source for the target.
#pragma once
#include "../abi/vm_abi.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace VmNativeHost {
namespace fs = std::filesystem;

struct Handle {
    bool open = false, directory = false;
    std::fstream file;
    fs::directory_iterator iterator, end;
};

inline Handle handles[24];
inline std::string packageRoot, contentPath;
inline uint8_t lastFailure;
inline uint32_t lastFailureDetail;
inline bool yieldRequested;

inline bool describe(const fs::path &path, VmFileInfo *info) {
    std::error_code error;
    const bool directory = fs::is_directory(path, error);
    if (error) return false;
    const auto name = path.filename().string();
    if (name.size() >= sizeof info->name) return false;
    std::memset(info, 0, sizeof *info);
    info->directory = directory;
    info->attributes = directory ? 16 : 32;
    if (!directory) {
        const auto bytes = fs::file_size(path, error);
        if (error || bytes > UINT32_MAX) return false;
        info->bytes = uint32_t(bytes);
    }
    std::memcpy(info->name, name.c_str(), name.size());
    return true;
}

// Mirrors VmRegistry::absolute: the loader refuses relative paths, traversal
// and backslashes before it ever touches the card.
inline bool acceptable(const char *path) {
    if (!path || path[0] != '/' || std::strlen(path) >= 384) return false;
    return !std::strstr(path, "..") && !std::strchr(path, '\\');
}

inline uint32_t openFlags(const char *path, uint32_t flags, VmFileInfo *info) {
    if (!acceptable(path) || !(flags & 3) || (flags & ~31u) ||
        ((flags & 28) && !(flags & VM_OPEN_WRITE))) return 0;
    std::error_code error;
    const fs::path target(path);
    const bool directory = fs::is_directory(target, error);
    if (!directory && !fs::exists(target, error) && !(flags & VM_OPEN_CREATE)) return 0;
    for (uint32_t i = 0; i < 24; ++i) {
        if (handles[i].open) continue;
        if (!describe(target, info)) return 0;
        handles[i] = Handle{};
        handles[i].open = true;
        handles[i].directory = directory;
        if (directory) {
            handles[i].iterator = fs::directory_iterator(target, error);
            if (error) { handles[i].open = false; return 0; }
        } else {
            auto mode = std::ios::binary | std::ios::in;
            if (flags & VM_OPEN_WRITE) mode |= std::ios::out;
            if (flags & VM_OPEN_TRUNCATE) mode |= std::ios::trunc;
            if ((flags & VM_OPEN_CREATE) && !fs::exists(target, error))
                std::ofstream(target, std::ios::binary);
            handles[i].file.open(target, mode);
            if (!handles[i].file) { handles[i].open = false; return 0; }
        }
        return i + 1;
    }
    return 0;  // All 24 handles in use, exactly as on target.
}

inline uint32_t openFile(const char *path, VmFileInfo *info) { return openFlags(path, VM_OPEN_READ, info); }

inline Handle *resolve(uint32_t handle) {
    if (!handle || handle > 24 || !handles[handle - 1].open) return nullptr;
    return &handles[handle - 1];
}

inline int32_t readFile(uint32_t handle, uint32_t offset, void *data, uint32_t count) {
    Handle *h = resolve(handle);
    if (!h || h->directory || count > INT32_MAX || count > UINT32_MAX - offset) return -1;
    h->file.clear();
    h->file.seekg(offset);
    if (!h->file) return -1;
    h->file.read(static_cast<char *>(data), count);
    return int32_t(h->file.gcount());
}

inline int32_t writeFile(uint32_t handle, uint32_t offset, const void *data, uint32_t count) {
    Handle *h = resolve(handle);
    if (!h || h->directory || count > INT32_MAX || count > UINT32_MAX - offset) return -1;
    h->file.clear();
    h->file.seekp(offset);
    if (!h->file) return -1;
    h->file.write(static_cast<const char *>(data), count);
    h->file.flush();
    return h->file ? int32_t(count) : -1;
}

inline int32_t nextFile(uint32_t handle, VmFileInfo *info) {
    Handle *h = resolve(handle);
    if (!h || !h->directory) return -1;
    while (h->iterator != h->end) {
        const auto entry = h->iterator->path();
        ++h->iterator;
        if (describe(entry, info)) return 1;
    }
    return 0;
}

inline void closeFile(uint32_t handle) {
    if (Handle *h = resolve(handle)) { h->file.close(); h->open = false; }
}

inline int32_t fileOp(VmFsRequest *request) {
    if (!request) return -1;
    std::error_code error;
    switch (request->operation) {
    case VmFsOp::Flush: { Handle *h = resolve(request->handle); if (!h) return -1; h->file.flush(); return 0; }
    case VmFsOp::Close: closeFile(request->handle); return 0;
    case VmFsOp::Mkdir: return acceptable(request->path) && fs::create_directories(request->path, error) ? 0 : -1;
    case VmFsOp::Rmdir:
    case VmFsOp::Remove: return acceptable(request->path) && fs::remove(request->path, error) ? 0 : -1;
    case VmFsOp::Rename:
        if (!acceptable(request->path) || !acceptable(request->destination)) return -1;
        fs::rename(request->path, request->destination, error);
        return error ? -1 : 0;
    case VmFsOp::Space: {
        const auto space = fs::space(packageRoot, error);
        if (error) return -1;
        request->handle = 1;
        request->value = uint32_t(space.capacity / 512 > UINT32_MAX ? UINT32_MAX : space.capacity / 512);
        request->extra = uint32_t(space.available / 512 > UINT32_MAX ? UINT32_MAX : space.available / 512);
        return 0;
    }
    default: return -1;
    }
}

inline uint32_t microsNow() {
    using namespace std::chrono;
    return uint32_t(duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

inline bool shouldYield() { return yieldRequested; }

inline void fail(uint8_t code, uint32_t detail) { lastFailure = code ? code : 0x16; lastFailureDetail = detail; }

// Builds the host exactly as VmRuntime::loadModule does, minus the MPU work.
inline VmHost make(const std::string &root, const std::string &content,
                   uint8_t *workspace, uint32_t workspaceBytes,
                   uint8_t *guest, uint32_t guestBytes) {
    for (auto &handle : handles) { handle.file.close(); handle.open = false; }
    packageRoot = root;
    contentPath = content;
    lastFailure = 0;
    lastFailureDetail = 0;
    yieldRequested = false;
    VmHost host{};
    host.abi = VM_ABI;
    host.bytes = sizeof(VmHost);
    host.services = VM_SERVICES;
    host.workspace = workspace;
    host.workspace_bytes = workspaceBytes;
    host.package_root = packageRoot.c_str();
    host.content_path = contentPath.c_str();
    host.micros_now = microsNow;
    host.open = openFile;
    host.read = readFile;
    host.next = nextFile;
    host.close = closeFile;
    host.guest_ram = guest;
    host.guest_ram_bytes = guestBytes;
    host.open_flags = openFlags;
    host.write = writeFile;
    host.file_op = fileOp;
    host.should_yield = shouldYield;
    host.fail = fail;
    return host;
}

}  // namespace VmNativeHost
