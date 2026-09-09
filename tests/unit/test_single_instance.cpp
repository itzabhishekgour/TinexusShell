#include <txui/core/SingleInstance.hpp>
#include <common/logger.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>

void test_primary_acquisition() {
    std::cout << "[Test 1] Testing primary single-instance acquisition..." << std::endl;
    std::string test_id = "test_primary_app_" + std::to_string(getpid());
    txui::SingleInstance primary(test_id);
    assert(primary.is_primary());
    assert(txui::SingleInstance::is_app_running(test_id));
    std::cout << "  -> Passed: Primary acquired exclusive lock.\n";
}

void test_secondary_rejection() {
    std::cout << "[Test 2] Testing secondary instance rejection..." << std::endl;
    std::string test_id = "test_secondary_app_" + std::to_string(getpid());
    txui::SingleInstance primary(test_id);
    assert(primary.is_primary());

    // Second instance attempting same ID
    txui::SingleInstance secondary(test_id);
    assert(!secondary.is_primary());
    std::cout << "  -> Passed: Secondary correctly rejected with EWOULDBLOCK.\n";
}

void test_release_on_destruction() {
    std::cout << "[Test 3] Testing release on normal destruction (without unlinking)..." << std::endl;
    std::string test_id = "test_release_app_" + std::to_string(getpid());
    {
        txui::SingleInstance primary(test_id);
        assert(primary.is_primary());
        assert(txui::SingleInstance::is_app_running(test_id));
    }
    // After destruction, lock is released (even though lock file remains on disk)
    assert(!txui::SingleInstance::is_app_running(test_id));
    txui::SingleInstance next_primary(test_id);
    assert(next_primary.is_primary());
    std::cout << "  -> Passed: Lock cleanly reclaimed without deleting file.\n";
}

void test_crash_recovery() {
    std::cout << "[Test 4] Testing crash recovery (SIGKILL cleanup by kernel)..." << std::endl;
    std::string test_id = "test_crash_app_" + std::to_string(getpid());

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child acquires lock
        close(pipefd[0]);
        txui::SingleInstance child_lock(test_id);
        assert(child_lock.is_primary());

        // Signal parent that lock is held
        char token = 'K';
        assert(write(pipefd[1], &token, 1) == 1);
        close(pipefd[1]);

        // Hang until killed
        while (true) {
            pause();
        }
        _exit(0);
    }

    // Parent waits for child to acquire
    close(pipefd[1]);
    char token = 0;
    assert(read(pipefd[0], &token, 1) == 1);
    close(pipefd[0]);

    // Verify lock is held by child
    assert(txui::SingleInstance::is_app_running(test_id));

    // Hard kill the child (SIGKILL, simulates sudden crash / OOM)
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);

    // Assert parent can immediately acquire the lock without manual cleanup
    txui::SingleInstance recovered_lock(test_id);
    assert(recovered_lock.is_primary());
    std::cout << "  -> Passed: Kernel automatically released lock after SIGKILL crash.\n";
}

void test_concurrent_launch_race() {
    std::cout << "[Test 5] Testing concurrent launch race (8 threads)..." << std::endl;
    std::string test_id = "test_race_app_" + std::to_string(getpid());
    constexpr int NUM_THREADS = 8;

    std::atomic<int> primary_count{0};
    std::atomic<int> secondary_count{0};
    std::atomic<bool> start_flag{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }
            txui::SingleInstance inst(test_id);
            if (inst.is_primary()) {
                primary_count.fetch_add(1);
                // Hold briefly so others see the lock
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                secondary_count.fetch_add(1);
            }
        });
    }

    start_flag.store(true);
    for (auto& t : threads) {
        t.join();
    }

    assert(primary_count.load() == 1);
    assert(secondary_count.load() == NUM_THREADS - 1);
    std::cout << "  -> Passed: Exactly 1 thread became primary, "
              << secondary_count.load() << " threads rejected as secondary.\n";
}

void test_multi_instance_independence() {
    std::cout << "[Test 6] Testing multi-instance terminal independence..." << std::endl;
    std::string term1 = "tinexus-terminal-window-1_" + std::to_string(getpid());
    std::string term2 = "tinexus-terminal-window-2_" + std::to_string(getpid());

    txui::SingleInstance inst1(term1);
    txui::SingleInstance inst2(term2);

    assert(inst1.is_primary());
    assert(inst2.is_primary());
    std::cout << "  -> Passed: Independent windows do not collide.\n";
}

int main() {
    tinexus::log::set_component_name("test_single_instance");
    std::cout << "========================================" << std::endl;
    std::cout << " RUNNING SINGLE INSTANCE ARCHITECTURE TESTS " << std::endl;
    std::cout << "========================================" << std::endl;

    test_primary_acquisition();
    test_secondary_rejection();
    test_release_on_destruction();
    test_crash_recovery();
    test_concurrent_launch_race();
    test_multi_instance_independence();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL SINGLE INSTANCE TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
