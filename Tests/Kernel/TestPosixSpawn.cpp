/*
 * Copyright (c) 2025, Tomás Simões <tomasprsimoes@tecnico.ulisboa.pt>
 * Copyright (c) 2026, Fırat Kızılboğa <firatkizilboga11@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <AK/StringView.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibTest/TestCase.h>
#include <fcntl.h>
#include <limits.h>
#include <spawn.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void spawn_and_wait(posix_spawn_file_actions_t* file_actions, posix_spawnattr_t* attr, char const* path, char* const argv[], int expected_exit_code = 0)
{
    pid_t pid;
    int rc = posix_spawn(&pid, path, file_actions, attr, argv, environ);
    EXPECT_EQ(rc, 0);

    int status;
    rc = waitpid(pid, &status, 0);
    EXPECT_EQ(rc, pid);
    EXPECT(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), expected_exit_code);
}

static ByteString read_file_content(char const* path)
{
    auto file_or_error = Core::File::open(StringView { path, strlen(path) }, Core::File::OpenMode::Read);
    EXPECT(!file_or_error.is_error());
    auto file = file_or_error.release_value();
    auto content_or_error = file->read_until_eof();
    EXPECT(!content_or_error.is_error());
    return ByteString::copy(content_or_error.value());
}

enum class UseSyscall {
    No,
    Yes,
};

static posix_spawnattr_t* spawnattr_for_syscall_mode(UseSyscall use_syscall, posix_spawnattr_t& attr)
{
    if (use_syscall == UseSyscall::Yes)
        return nullptr;

    // FIXME: Stop testing this non-syscall fallback once spawn attributes are supported by the posix_spawn syscall.
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, 0);
    return &attr;
}

static void cleanup_attr_for_syscall_mode(UseSyscall use_syscall, posix_spawnattr_t& attr)
{
    if (use_syscall == UseSyscall::No)
        posix_spawnattr_destroy(&attr);
}

static void test_spawn_without_file_actions_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(nullptr, attr_ptr, "/bin/true", argv, 0);

    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_addopen_redirect_stdout_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char path[] = "/tmp/spawn_test_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd >= 0);
    close(fd);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, path, O_WRONLY | O_TRUNC, 0644), 0);

    char* argv[] = { const_cast<char*>("/bin/echo"), const_cast<char*>("hello"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/echo", argv, 0);

    auto content = read_file_content(path);
    EXPECT_EQ(content.trim_whitespace(), "hello");

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_addopen_redirect_stdin_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char path[] = "/tmp/spawn_test_in_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd >= 0);
    ByteString input_data = "data_from_file";
    write(fd, input_data.characters(), input_data.length());
    close(fd);

    char out_path[] = "/tmp/spawn_test_out_XXXXXX";
    fd = mkstemp(out_path);
    EXPECT(fd >= 0);
    close(fd);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, path, O_RDONLY, 0), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, out_path, O_WRONLY | O_TRUNC, 0644), 0);

    char* argv[] = { const_cast<char*>("/bin/cat"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/cat", argv, 0);

    auto content = read_file_content(out_path);
    EXPECT_EQ(content, input_data);

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
    unlink(out_path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_adddup2_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char path[] = "/tmp/spawn_dup2_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd >= 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, fd, STDOUT_FILENO), 0);

    char* argv[] = { const_cast<char*>("/bin/echo"), const_cast<char*>("dup2_test"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/echo", argv, 0);

    close(fd);
    auto content = read_file_content(path);
    EXPECT_EQ(content.trim_whitespace(), "dup2_test");

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_adddup2_same_fd_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDOUT_FILENO), 0);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/true", argv, 0);

    posix_spawn_file_actions_destroy(&actions);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_addclose_stdin_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addclose(&actions, STDIN_FILENO), 0);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/true", argv, 0);

    posix_spawn_file_actions_destroy(&actions);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_addchdir_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char out_path[] = "/tmp/spawn_cwd_XXXXXX";
    int fd = mkstemp(out_path);
    EXPECT(fd >= 0);
    close(fd);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addchdir(&actions, "/tmp"), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, out_path, O_WRONLY | O_TRUNC, 0644), 0);

    char* argv[] = { const_cast<char*>("/bin/pwd"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/pwd", argv, 0);

    auto content = read_file_content(out_path);
    EXPECT_EQ(content.trim_whitespace(), "/tmp");

    posix_spawn_file_actions_destroy(&actions);
    unlink(out_path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_addfchdir_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    int dir_fd = open("/tmp", O_RDONLY | O_DIRECTORY);
    EXPECT(dir_fd >= 0);

    char out_path[] = "/tmp/spawn_fchdir_XXXXXX";
    int fd = mkstemp(out_path);
    EXPECT(fd >= 0);
    close(fd);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addfchdir(&actions, dir_fd), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, out_path, O_WRONLY | O_TRUNC, 0644), 0);

    char* argv[] = { const_cast<char*>("/bin/pwd"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/pwd", argv, 0);

    auto content = read_file_content(out_path);
    EXPECT_EQ(content.trim_whitespace(), "/tmp");

    posix_spawn_file_actions_destroy(&actions);
    close(dir_fd);
    unlink(out_path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_multiple_actions_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char path[] = "/tmp/spawn_seq_XXXXXX";
    int dummy = mkstemp(path);
    EXPECT(dummy >= 0);
    close(dummy);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);

    int target_fd = 10;
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, target_fd, path, O_WRONLY | O_TRUNC, 0644), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, target_fd, STDOUT_FILENO), 0);
    EXPECT_EQ(posix_spawn_file_actions_addclose(&actions, target_fd), 0);

    char* argv[] = { const_cast<char*>("/bin/echo"), const_cast<char*>("sequence"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/echo", argv, 0);

    auto content = read_file_content(path);
    EXPECT_EQ(content.trim_whitespace(), "sequence");

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_high_fd_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char path[] = "/tmp/spawn_highfd_XXXXXX";
    int dummy = mkstemp(path);
    EXPECT(dummy >= 0);
    close(dummy);

    int high_fd = 100;

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, high_fd, path, O_WRONLY | O_TRUNC, 0644), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, high_fd, STDOUT_FILENO), 0);

    char* argv[] = { const_cast<char*>("/bin/echo"), const_cast<char*>("high"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/echo", argv, 0);

    auto content = read_file_content(path);
    EXPECT_EQ(content.trim_whitespace(), "high");

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_parent_unchanged_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    int start_fds = 0;
    for (int i = 0; i < 1024; ++i) {
        if (fcntl(i, F_GETFD) != -1)
            start_fds++;
    }

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, 20, "/dev/null", O_RDONLY, 0), 0);
    EXPECT_EQ(posix_spawn_file_actions_addclose(&actions, STDOUT_FILENO), 0);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/true", argv, 0);
    posix_spawn_file_actions_destroy(&actions);

    int end_fds = 0;
    for (int i = 0; i < 1024; ++i) {
        if (fcntl(i, F_GETFD) != -1)
            end_fds++;
    }
    EXPECT_EQ(start_fds, end_fds);

    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

static void test_parent_cwd_unchanged_impl(UseSyscall use_syscall)
{
    posix_spawnattr_t attr;
    auto* attr_ptr = spawnattr_for_syscall_mode(use_syscall, attr);

    char original_cwd[PATH_MAX];
    EXPECT(getcwd(original_cwd, sizeof(original_cwd)) != nullptr);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addchdir(&actions, "/tmp"), 0);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(&actions, attr_ptr, "/bin/true", argv, 0);
    posix_spawn_file_actions_destroy(&actions);

    char new_cwd[PATH_MAX];
    EXPECT(getcwd(new_cwd, sizeof(new_cwd)) != nullptr);
    EXPECT_EQ(strcmp(original_cwd, new_cwd), 0);

    cleanup_attr_for_syscall_mode(use_syscall, attr);
}

TEST_CASE(fast_spawn_without_file_actions) { test_spawn_without_file_actions_impl(UseSyscall::Yes); }
TEST_CASE(fast_addopen_redirect_stdout) { test_addopen_redirect_stdout_impl(UseSyscall::Yes); }
TEST_CASE(fast_addopen_redirect_stdin) { test_addopen_redirect_stdin_impl(UseSyscall::Yes); }
TEST_CASE(fast_adddup2) { test_adddup2_impl(UseSyscall::Yes); }
TEST_CASE(fast_adddup2_same_fd) { test_adddup2_same_fd_impl(UseSyscall::Yes); }
TEST_CASE(fast_addclose_stdin) { test_addclose_stdin_impl(UseSyscall::Yes); }
TEST_CASE(fast_addchdir) { test_addchdir_impl(UseSyscall::Yes); }
TEST_CASE(fast_addfchdir) { test_addfchdir_impl(UseSyscall::Yes); }
TEST_CASE(fast_multiple_actions) { test_multiple_actions_impl(UseSyscall::Yes); }
TEST_CASE(fast_high_fd) { test_high_fd_impl(UseSyscall::Yes); }
TEST_CASE(fast_parent_unchanged) { test_parent_unchanged_impl(UseSyscall::Yes); }
TEST_CASE(fast_parent_cwd_unchanged) { test_parent_cwd_unchanged_impl(UseSyscall::Yes); }

TEST_CASE(slow_spawn_without_file_actions) { test_spawn_without_file_actions_impl(UseSyscall::No); }
TEST_CASE(slow_addopen_redirect_stdout) { test_addopen_redirect_stdout_impl(UseSyscall::No); }
TEST_CASE(slow_addopen_redirect_stdin) { test_addopen_redirect_stdin_impl(UseSyscall::No); }
TEST_CASE(slow_adddup2) { test_adddup2_impl(UseSyscall::No); }
TEST_CASE(slow_adddup2_same_fd) { test_adddup2_same_fd_impl(UseSyscall::No); }
TEST_CASE(slow_addclose_stdin) { test_addclose_stdin_impl(UseSyscall::No); }
TEST_CASE(slow_addchdir) { test_addchdir_impl(UseSyscall::No); }
TEST_CASE(slow_addfchdir) { test_addfchdir_impl(UseSyscall::No); }
TEST_CASE(slow_multiple_actions) { test_multiple_actions_impl(UseSyscall::No); }
TEST_CASE(slow_high_fd) { test_high_fd_impl(UseSyscall::No); }
TEST_CASE(slow_parent_unchanged) { test_parent_unchanged_impl(UseSyscall::No); }
TEST_CASE(slow_parent_cwd_unchanged) { test_parent_cwd_unchanged_impl(UseSyscall::No); }

TEST_CASE(error_enoent_for_missing_file)
{
    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/does/not/exist", O_RDONLY, 0), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, ENOENT);

    posix_spawn_file_actions_destroy(&actions);
}

TEST_CASE(error_enoent_for_missing_directory)
{
    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addchdir(&actions, "/does/not/exist/dir"), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, ENOENT);

    posix_spawn_file_actions_destroy(&actions);
}

TEST_CASE(error_enotdir_for_fchdir_on_file)
{
    char path[] = "/tmp/spawn_not_dir_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd >= 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addfchdir(&actions, fd), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, ENOTDIR);

    posix_spawn_file_actions_destroy(&actions);
    close(fd);
    unlink(path);
}

TEST_CASE(error_ebadf_for_invalid_dup2_source)
{
    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, 999, STDOUT_FILENO), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, EBADF);

    posix_spawn_file_actions_destroy(&actions);
}

TEST_CASE(error_ebadf_for_invalid_close)
{
    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addclose(&actions, 999), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, EBADF);

    posix_spawn_file_actions_destroy(&actions);
}

TEST_CASE(error_eacces_for_fchdir_no_permission)
{
    char dir_path[] = "/tmp/spawn_noexec_XXXXXX";
    EXPECT(mkdtemp(dir_path) != nullptr);
    EXPECT_EQ(chmod(dir_path, 0600), 0);

    int dir_fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    EXPECT(dir_fd >= 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addfchdir(&actions, dir_fd), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, EACCES);

    posix_spawn_file_actions_destroy(&actions);
    close(dir_fd);
    rmdir(dir_path);
}

TEST_CASE(action_order_matters)
{
    char path[] = "/tmp/spawn_order_XXXXXX";
    int dummy = mkstemp(path);
    EXPECT(dummy >= 0);
    close(dummy);

    int target_fd = 15;

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, target_fd, STDOUT_FILENO), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, target_fd, path, O_WRONLY | O_TRUNC, 0644), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, nullptr, argv, environ);
    EXPECT_EQ(rc, EBADF);

    posix_spawn_file_actions_destroy(&actions);
    unlink(path);
}

TEST_CASE(empty_file_actions)
{
    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);

    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };
    spawn_and_wait(&actions, nullptr, "/bin/true", argv, 0);

    posix_spawn_file_actions_destroy(&actions);
}

TEST_CASE(slow_error_enoent_for_missing_file)
{
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/does/not/exist", O_RDONLY, 0), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    // Slow path: spawn succeeds but child exits with 127
    int rc = posix_spawn(&pid, "/bin/true", &actions, &attr, argv, environ);
    EXPECT_EQ(rc, 0);

    int status;
    waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 127);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
}

TEST_CASE(slow_error_enoent_for_missing_directory)
{
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addchdir(&actions, "/does/not/exist/dir"), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, &attr, argv, environ);
    EXPECT_EQ(rc, 0);

    int status;
    waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 127);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
}

TEST_CASE(slow_error_ebadf_for_invalid_dup2)
{
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_adddup2(&actions, 999, STDOUT_FILENO), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, &attr, argv, environ);
    EXPECT_EQ(rc, 0);

    int status;
    waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 127);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
}

TEST_CASE(slow_error_ebadf_for_invalid_close)
{
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, 0);

    posix_spawn_file_actions_t actions;
    EXPECT_EQ(posix_spawn_file_actions_init(&actions), 0);
    EXPECT_EQ(posix_spawn_file_actions_addclose(&actions, 999), 0);

    pid_t pid;
    char* argv[] = { const_cast<char*>("/bin/true"), nullptr };

    int rc = posix_spawn(&pid, "/bin/true", &actions, &attr, argv, environ);
    EXPECT_EQ(rc, 0);

    int status;
    waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 127);

    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
}
