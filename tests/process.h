#include "unistd.h"
#include "wait.h"
#include <cassert>
#include <experimental/filesystem>
#include <optional>
#include <signal.h>

namespace fs = std::experimental::filesystem;

namespace h5s3::testing {


/** Class for managing the lifetime of an external process.

    This class creates a new process via fork/exec upon construction. Callers
    **MUST** call `join()` before this object goes out of scope. If this object
    will call `std::terminate()` if it is destroyed before its managed process
    exits.

    @param args Vector of strings to pass as the command line of the process.
    @param env Vector of string key-value pairs to pass as the process environment.
 */
class process {

private:
    std::vector<std::string> m_argv;
    std::vector<std::string> m_envp;
    pid_t m_pid;

public:
    using environment = std::vector<std::pair<std::string, std::string>>;

    enum class state {
        RUNNING,
        EXITED,
        SIGNALED,
    };

    class status {
    public:
        int code;
        state process_state;
    };

    class error : public std::runtime_error {
    public:
        explicit error(const std::string& message) : std::runtime_error(message) {}
    };

    process(const std::vector<std::string>& args,
            const environment& env) : m_argv(args) {

        // Initialize argv.
        std::vector<char *> c_args;
        for (auto& str : m_argv) {
            c_args.push_back(str.data());
        }
        c_args.push_back(nullptr);

        // Initialize envp.
        for (size_t i = 0; i < env.size(); ++i) {
            std::stringstream fmt;
            auto& [k, v] = env[i];
            fmt << k << "=" << v;
            m_envp.emplace_back(fmt.str());
        }
        std::vector<char *> c_env;
        for (auto& str : m_envp) {
            c_env.emplace_back(str.data());
        }
        c_env.push_back(nullptr);

        // Do the fork/exect.
        m_pid = fork();
        if (-1 == m_pid) {
            throw error("Failed to fork new process.");
        }
        if(!m_pid) {
            // We're the child.
            execvpe(c_args[0], c_args.data(), c_env.data());
            throw error("Failed to exec new process.");
        }
    }

    ~process() {
        try {
            if (running()) {
                kill(m_pid, SIGKILL);
                std::terminate();
            }
        }
        catch (const error& e) {
        }
    }

    /** Check if the process is running.
     */
    bool running() {
        if (kill(m_pid, 0)) {
            if (ESRCH == errno) {
                return false;
            }
        }
        return true;
    }

    /** Send a SIGINT to the process.
     */
    void interrupt() {
        if (kill(m_pid, SIGINT)) {
            if (ESRCH != errno) {
                throw error("Failed to send SIGINT.");
            }
        }
    }

    /** Send a SIGTERM to the process.
     */
    void terminate() {
        if (kill(m_pid, SIGTERM)) {
            if (ESRCH != errno) {
                throw error("Failed to send SIGTERM.");
            }
        }
    }

    /** Wait for the process to exit.

        @return A struct indicating the exit status of the process and the
                cause of exit.

                If the process exited normally, status.process_state will be
                `process::state::EXITED`, status.code will be set to the
                process' exit code.

                If the process exited due to receipt of a signal,
                status.process_state will be `process::state::SIGNALED`, and
                status.code will be the number of the signal that caused
                termination.
     */
    status join() {
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

    /** The pid of the managed process.
     */
    pid_t pid() const {
        return m_pid;
    }
};

} // namespace h5s3::testing
