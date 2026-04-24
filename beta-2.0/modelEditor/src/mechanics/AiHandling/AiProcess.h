#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace AI {

// Owns a child process (the Python agent sidecar) and an IO worker thread.
// - send(line): writes a single line of JSON to the child's stdin.
// - drainInbox(cb): called from main thread; invokes cb for each stdout line
//   the worker has buffered since the last drain.
//
// Thread model:
//   - ctor starts the child process and a reader thread.
//   - Reader thread: blocks on stdout, pushes each line onto mInbox.
//   - Main thread: calls drainInbox() per frame; sends lines via send().
class AiProcess {
public:
    AiProcess();
    ~AiProcess();

    // Launch the child. pythonExe = "python" or absolute path. scriptPath is
    // the path to agent.py (absolute). Returns true on success.
    bool start(const std::string& pythonExe, const std::string& scriptPath);

    // Send one line of JSON to the child (newline appended if missing).
    // Safe to call from main thread only.
    bool send(const std::string& line);

    // Pop all buffered stdout lines and invoke cb for each.
    void drainInbox(const std::function<void(const std::string&)>& cb);

    // Returns true if the child is still running.
    bool alive() const;

    // Signal shutdown, close pipes, wait for child to exit, join reader.
    void stop();

private:
    void readerLoop();

    std::atomic<bool> mRunning{false};

#ifdef _WIN32
    void* mHProcess = nullptr;   // HANDLE
    void* mHStdinW = nullptr;    // HANDLE (we write)
    void* mHStdoutR = nullptr;   // HANDLE (we read)
#endif

    std::thread mReader;
    std::mutex mInboxMu;
    std::deque<std::string> mInbox;
};

} // namespace AI
