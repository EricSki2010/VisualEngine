#include "AiProcess.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <iostream>
#include <vector>

namespace AI {

AiProcess::AiProcess() = default;

AiProcess::~AiProcess() {
    stop();
}

bool AiProcess::start(const std::string& pythonExe, const std::string& scriptPath) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdoutR = nullptr, stdoutW = nullptr;
    HANDLE stdinR = nullptr,  stdinW = nullptr;

    if (!CreatePipe(&stdoutR, &stdoutW, &sa, 0)) return false;
    if (!SetHandleInformation(stdoutR, HANDLE_FLAG_INHERIT, 0)) { CloseHandle(stdoutR); CloseHandle(stdoutW); return false; }
    if (!CreatePipe(&stdinR, &stdinW, &sa, 0)) { CloseHandle(stdoutR); CloseHandle(stdoutW); return false; }
    if (!SetHandleInformation(stdinW, HANDLE_FLAG_INHERIT, 0)) { CloseHandle(stdoutR); CloseHandle(stdoutW); CloseHandle(stdinR); CloseHandle(stdinW); return false; }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinR;
    si.hStdOutput = stdoutW;
    si.hStdError = stdoutW; // merge stderr into stdout for simpler logging
    PROCESS_INFORMATION pi{};

    // Build command line: "python" "scriptPath"
    std::string cmd = "\"" + pythonExe + "\" -u \"" + scriptPath + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr,
        TRUE,            // inherit handles
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);

    // Close child-side ends we don't need in parent.
    CloseHandle(stdoutW);
    CloseHandle(stdinR);

    if (!ok) {
        CloseHandle(stdoutR);
        CloseHandle(stdinW);
        std::cerr << "[AI] CreateProcess failed, error=" << GetLastError() << "\n";
        return false;
    }

    CloseHandle(pi.hThread);
    mHProcess = pi.hProcess;
    mHStdinW = stdinW;
    mHStdoutR = stdoutR;
    mRunning = true;

    mReader = std::thread([this]{ readerLoop(); });
    return true;
#else
    (void)pythonExe; (void)scriptPath;
    return false;
#endif
}

bool AiProcess::send(const std::string& line) {
#ifdef _WIN32
    if (!mRunning || !mHStdinW) return false;
    std::string out = line;
    if (out.empty() || out.back() != '\n') out.push_back('\n');
    DWORD written = 0;
    BOOL ok = WriteFile(mHStdinW, out.data(), (DWORD)out.size(), &written, nullptr);
    return ok && written == out.size();
#else
    (void)line;
    return false;
#endif
}

void AiProcess::readerLoop() {
#ifdef _WIN32
    std::string buf;
    char chunk[4096];
    while (mRunning) {
        DWORD read = 0;
        BOOL ok = ReadFile(mHStdoutR, chunk, sizeof(chunk), &read, nullptr);
        if (!ok || read == 0) break;
        buf.append(chunk, chunk + read);
        // Split on newlines.
        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            {
                std::lock_guard<std::mutex> lock(mInboxMu);
                mInbox.push_back(std::move(line));
            }
        }
    }
    mRunning = false;
#endif
}

void AiProcess::drainInbox(const std::function<void(const std::string&)>& cb) {
    std::deque<std::string> local;
    {
        std::lock_guard<std::mutex> lock(mInboxMu);
        local.swap(mInbox);
    }
    for (auto& line : local) cb(line);
}

bool AiProcess::alive() const {
    return mRunning.load();
}

void AiProcess::stop() {
#ifdef _WIN32
    if (!mRunning && !mHProcess) return;
    mRunning = false;

    if (mHStdinW) { CloseHandle(mHStdinW); mHStdinW = nullptr; }
    if (mHProcess) {
        WaitForSingleObject(mHProcess, 2000);
        TerminateProcess(mHProcess, 0);
        CloseHandle(mHProcess);
        mHProcess = nullptr;
    }
    if (mHStdoutR) { CloseHandle(mHStdoutR); mHStdoutR = nullptr; }
    if (mReader.joinable()) mReader.join();
#endif
}

} // namespace AI
