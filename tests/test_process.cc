#include "gtest/gtest.h"

#include "h5s3/private/process.h"

using process = h5s3::utils::process;

TEST(process, clean_exit) {
    std::vector<std::string> args = {"python", "-c", "exit(0)"};
    process::environment env = {};
    process proc(args, env);

    auto[code, state] = proc.join();
    EXPECT_EQ(code, 0);
    EXPECT_EQ(state, process::state::EXITED);
}

TEST(process, error_exit) {
    std::vector<std::string> args = {"python", "-c", "exit(1)"};
    process::environment env = {};
    process proc(args, env);

    auto[code, state] = proc.join();
    EXPECT_EQ(code, 1);
    EXPECT_EQ(state, process::state::EXITED);
}

TEST(process, environment) {
    std::vector<std::string> args = {"python",
                                     "-c",
                                     "import os; assert os.environ['foo'] == 'bar'"};
    process::environment env = {{"foo", "bar"}};
    process proc(args, env);

    auto[code, state] = proc.join();
    EXPECT_EQ(code, 0);
    EXPECT_EQ(state, process::state::EXITED);
}

TEST(process, interrupt) {
    std::vector<std::string> args = {"python", "-c", "import time; time.sleep(10)"};
    process::environment env = {};
    process proc(args, env);

    EXPECT_TRUE(proc.running());
    proc.interrupt();

    auto[code, state] = proc.join();
    EXPECT_EQ(code, SIGINT);
    EXPECT_EQ(state, process::state::SIGNALED);
}

TEST(process, twice) {
    std::vector<std::string> args = {"python", "-c", "import time; time.sleep(10)"};
    process::environment env = {};
    process proc(args, env);

    EXPECT_TRUE(proc.running());
    proc.interrupt();
    {
        auto[code, state] = proc.join();
        EXPECT_EQ(code, SIGINT);
        EXPECT_EQ(state, process::state::SIGNALED);
        EXPECT_FALSE(proc.running());
    }

    // Joining for a second time should trigger an error.
    EXPECT_THROW({ proc.join(); }, process::error);
}

TEST(process, terminate) {
    std::vector<std::string> args = {"python", "-c", "import time; time.sleep(10)"};

    process::environment env = {};
    process proc(args, env);

    EXPECT_TRUE(proc.running());
    proc.terminate();

    auto[code, state] = proc.join();
    EXPECT_EQ(code, SIGTERM);
    EXPECT_EQ(state, process::state::SIGNALED);
    EXPECT_FALSE(proc.running());
}

void fail_to_clean_up_process() {
    std::vector<std::string> args = {"python", "-c", "import time; time.sleep(10)"};
    process::environment env = {};
    process proc(args, env);
}

TEST(process, blow_up_the_world) {
    ASSERT_DEATH(fail_to_clean_up_process(),
                 "terminate called without an active exception");
}
