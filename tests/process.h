#include "unistd.h"
#include "wait.h"
#include <cassert>
#include <experimental/filesystem>
#include <optional>
#include <signal.h>

namespace fs = std::experimental::filesystem;

namespace h5s3::testing {

class process {

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

private:
    std::vector<std::string> m_argv;
    std::vector<std::string> m_envp;
    pid_t m_pid;

public:
    process(std::vector<std::string>& args,
            environment& env) {

        // Initialize argv.
        for (unsigned int i = 0; i < args.size(); ++i) {
            m_argv.emplace_back(args[i]);
        }
        std::vector<char *> c_args;
        for (auto& str : m_argv) {
            c_args.push_back(str.data());
        }
        c_args.push_back(NULL);

        // Initialize envp.
        for (unsigned int i = 0; i < env.size(); ++i) {
            std::stringstream fmt;
            auto& [k, v] = env[i];
            fmt << k << "=" << v;
            m_envp.emplace_back(fmt.str());
        }
        std::vector<char *> c_env;
        for (auto& str : m_envp) {
            c_env.emplace_back(str.data());
        }
        c_env.push_back(NULL);


        // fork/exec
        pid_t result = fork();
        if (-1 == result) {
            throw error("Failed to fork new process.");
        }

        if(!result) {
            // We're the child;
            execvpe(c_args[0], c_args.data(), c_env.data());
            throw error("Failed to exec new process.");
        }
        m_pid = result;
    }

    void interrupt() {
        if (kill(m_pid, SIGINT)) {
            if(ESRCH != errno) {
                throw error("Failed to send SIGINT.");
            }
        }
    }

    void terminate() {
        if (kill(m_pid, SIGTERM)) {
            if(ESRCH != errno) {
                throw error("Failed to send SIGTERM.");
            }
        }
    }

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

    pid_t pid() const {
        return m_pid;
    }
};

} // namespace h5s3::testing
