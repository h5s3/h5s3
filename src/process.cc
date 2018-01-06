#include "h5s3/private/process.h"

namespace h5s3::utils {
process::process(const std::vector<std::string>& args, const environment& env)
    : m_argv(args) {

    // Initialize argv.
    std::vector<char*> c_args;
    for (auto& str : m_argv) {
        c_args.push_back(str.data());
    }
    c_args.push_back(nullptr);

    // Initialize envp.
    for (size_t i = 0; i < env.size(); ++i) {
        std::stringstream fmt;
        auto & [ k, v ] = env[i];
        fmt << k << "=" << v;
        m_envp.emplace_back(fmt.str());
    }
    std::vector<char*> c_env;
    for (auto& str : m_envp) {
        c_env.emplace_back(str.data());
    }
    c_env.push_back(nullptr);

    // Do the fork/exec.
    m_pid = fork();
    if (-1 == m_pid) {
        throw error("Failed to fork new process.");
    }
    if (!m_pid) {
        // We're the child.
        execvpe(c_args[0], c_args.data(), c_env.data());
        throw error("Failed to exec new process.");
    }
}

process::~process() {
    try {
        if (running()) {
            kill(m_pid, SIGKILL);
            std::terminate();
        }
    }
    catch (const error&) {
    }
}

bool process::running() {
    if (kill(m_pid, 0)) {
        if (ESRCH == errno) {
            return false;
        }
    }
    return true;
}

void process::interrupt() {
    if (kill(m_pid, SIGINT)) {
        if (ESRCH != errno) {
            throw error("Failed to send SIGINT.");
        }
    }
}

void process::terminate() {
    if (kill(m_pid, SIGTERM)) {
        if (ESRCH != errno) {
            throw error("Failed to send SIGTERM.");
        }
    }
}

process::status process::join() {
    int status;
    pid_t result = waitpid(m_pid, &status, 0);
    if (-1 == result) {
        throw error("Failed to wait for process.");
    }
    if (WIFEXITED(status)) {
        return {WEXITSTATUS(status), state::EXITED};
    }
    else {
        assert(WIFSIGNALED(status));
        return {WTERMSIG(status), state::SIGNALED};
    }
}
}  // namespace h5s3::utils
