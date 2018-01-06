#pragma once

#include <cassert>
#include <experimental/filesystem>
#include <optional>

#include <signal.h>
#include <unistd.h>
#include <wait.h>

namespace h5s3::utils {
/** Class for managing the lifetime of an external process.

    This class creates a new process via fork/exec upon construction. Callers
    **MUST** call `join()` before this object goes out of scope. If this object
    will call `std::terminate()` if it is destroyed before its managed process
    exits.

    @param args Vector of strings to pass as the command line of the process.
    @param env Vector of string key-value pairs to pass as the process environment.
 */
class process final {

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
        inline explicit error(const std::string& message) : std::runtime_error(message) {}
    };

    process(const std::vector<std::string>& args, const environment& env);

    ~process();

    /** Check if the process is running.
     */
    bool running();

    /** Send a SIGINT to the process.
     */
    void interrupt();

    /** Send a SIGTERM to the process.
     */
    void terminate();

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
    status join();

    /** The pid of the managed process.
     */
    inline pid_t pid() const {
        return m_pid;
    }
};
} // namespace h5s3::utils
