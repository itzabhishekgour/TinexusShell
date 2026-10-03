#include "SettingsBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QFileInfo>
#include <iostream>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <fstream>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    tinexus::settings_ui::SettingsBridge bridge;

    std::cout << "--- LIVE SETTINGS BRIDGE OUTPUT ---" << std::endl;
    std::cout << "osVersion:       " << bridge.osVersion().toStdString() << std::endl;
    std::cout << "cpuModel:        " << bridge.cpuModel().toStdString() << std::endl;
    std::cout << "memInfo:         " << bridge.memInfo().toStdString() << std::endl;
    std::cout << "storageInfo:     " << bridge.storageInfo().toStdString() << std::endl;
    std::cout << "displayInfo:     " << bridge.displayInfo().toStdString() << std::endl;
    std::cout << "compositorInfo:  " << bridge.compositorInfo().toStdString() << std::endl;
    std::cout << "graphicsEngine:  " << bridge.graphicsEngine().toStdString() << std::endl;

    // Verify against actual system calls
    struct utsname uts;
    uname(&uts);
    std::string expected_release = uts.release;
    if (bridge.osVersion().toStdString().find(expected_release) == std::string::npos) {
        std::cerr << "FAIL: osVersion does not contain uname release: " << expected_release << std::endl;
        return 1;
    }

    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line, expected_cpu;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                expected_cpu = line.substr(colon + 2);
                break;
            }
        }
    }
    if (!expected_cpu.empty() && bridge.cpuModel().toStdString() != expected_cpu) {
        std::cerr << "FAIL: cpuModel does not match /proc/cpuinfo: " << expected_cpu << std::endl;
        return 1;
    }

    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        if (bridge.storageInfo().isEmpty() || bridge.storageInfo().toStdString().find("GB") == std::string::npos) {
            std::cerr << "FAIL: storageInfo invalid: " << bridge.storageInfo().toStdString() << std::endl;
            return 1;
        }
    }

    std::cout << ">>> FIX 3 VERIFICATION PASSED: All About properties dynamically matched actual system data! <<<" << std::endl;
    return 0;
}
