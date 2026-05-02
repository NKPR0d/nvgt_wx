// NVGT plugin entry point. The actual binding lives in src/*.cpp; this
// translation unit only owns plugin_main(), which prepare_plugin's the
// shared engine pointer and then asks register.cpp to install everything.

#include "src/common.h"

plugin_main(nvgt_plugin_shared* shared) {
    if (!prepare_plugin(shared)) return false;
    register_all_types(shared->script_engine);
    return true;
}
