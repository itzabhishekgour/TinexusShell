#include "common/logger.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <csignal>
#include <filesystem>

using namespace tinexus;

void launch_component(const std::string& name) {
    pid_t pid = fork();
    if (pid == 0) {
        // Look for the binary in standard locations or debug build
        std::vector<std::string> search_paths = {
            std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/tinexus/build/debug/src/" + name + "/tinexus-" + name,
            "/usr/bin/tinexus-" + name
        };
        
        for (const auto& path : search_paths) {
            if (std::filesystem::exists(path)) {
                execl(path.c_str(), ("tinexus-" + name).c_str(), nullptr);
            }
        }
        
        // Fallback to PATH
        execlp(("tinexus-" + name).c_str(), ("tinexus-" + name).c_str(), nullptr);
        _exit(127);
    }
}

int main(int argc, char* argv[]) {
    log::set_component_name("session");
    log::info("Phase A: Tinexus Desktop Session starting...");

    // Milestone 1: Start wallpaper (Blue background)
    // Actually handled by compositor's static wlr_scene_rect for now.
    
    // Milestone 2 & 3: We will uncomment these as we hit the milestones
    launch_component("panel");
    launch_component("launcher");

    // Wait forever and reap zombies
    while (true) {
        int status;
        pid_t p = waitpid(-1, &status, 0);
        if (p > 0) {
            log::warn("A shell component exited.");
        }
    }

    return 0;
}
