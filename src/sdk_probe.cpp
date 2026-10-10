// SPDX-License-Identifier: Apache-2.0
#include "hik_bridge/bridge.hpp"

#include <dlfcn.h>
#include <stdexcept>

namespace hik_bridge {
void reject_unverified_sdk(const std::string& path) {
    if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos)
        throw std::invalid_argument("SDK path must be absolute");
    void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!library) throw std::runtime_error("SDK library unavailable");
    const char* missing = nullptr;
    const char* symbols[] = {
        "NET_DVR_Init", "NET_DVR_Cleanup", "NET_DVR_GetSDKVersion",
        "NET_DVR_GetLastError", "NET_DVR_Login_V40", "NET_DVR_Logout",
        "NET_DVR_SetDVRMessageCallBack_V31", "NET_DVR_SetExceptionCallBack_V30",
        "NET_DVR_SetupAlarmChan_V41", "NET_DVR_CloseAlarmChan_V30"
    };
    for (const char* name : symbols) {
        dlerror();
        void* address = dlsym(library, name);
        if (dlerror() || !address) { missing = name; break; }
    }
    const bool closed = dlclose(library) == 0;
    if (!closed) throw std::runtime_error("SDK library unload failed");
    if (missing) throw std::runtime_error(std::string("SDK missing symbol: ") + missing);
    // shortcut: no verified Linux ABI/version, add native calls only after documented acceptance.
    throw std::runtime_error("real SDK disabled: ABI and version are not verified");
}
} // namespace hik_bridge
