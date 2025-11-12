// z2079516.cxx
// PE1: append to a no-permissions file using UNIX system calls
// Uses: open(), close(), write(), stat(), chmod(), perror()
// Build: g++ -Wall -Wextra -std=c++17 -o z2079516 z2079516.cxx

#include <unistd.h>     // write, close
#include <fcntl.h>      // open, O_*
#include <sys/stat.h>   // stat, chmod
#include <sys/types.h>
#include <errno.h>
#include <string.h>     // strcmp, strlen
#include <stdio.h>      // fprintf, perror

static void usage(const char* p) {
    fprintf(stderr,
        "Usage: %s [-c] out_file message_string\n"
        "  where the message_string is appended to out_file.\n"
        "  The -c option clears the file before the message is appended.\n",
        p);
}

// tiny helper: ensure all bytes are written (simple & safe)
static int write_all(int fd, const char* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        ssize_t w = write(fd, s + i, n - i);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        i += (size_t)w;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    bool clear = false;
    const char* path = nullptr;
    const char* msg  = nullptr;

    // parse: optional -c, then out_file, then message_string
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0) clear = true;
        else if (!path) path = argv[i];
        else if (!msg)  msg  = argv[i];
    }
    if (!path || !msg) { usage(argv[0]); return 1; }

    struct stat st;
    // If file missing, create with 000 perms, then close.
    if (stat(path, &st) < 0) {
        if (errno != ENOENT) { perror("stat"); return 2; }
        int cfd = open(path, O_WRONLY | O_CREAT | O_EXCL, 000);
        if (cfd < 0) { perror("open(create)"); return 3; }
        if (close(cfd) < 0) { perror("close(create)"); return 4; }
        if (stat(path, &st) < 0) { perror("stat(after create)"); return 5; }
    }

    // Must be 000 before we touch it
    if ((st.st_mode & 0777) != 000) {
        fprintf(stderr, "%s is not secure. Ignoring.\n", path);
        return 6;
    }

    // allow user write temporarily
    if (chmod(path, 0200) < 0) { perror("chmod +w"); return 7; }

    // open for write: truncate if -c else append
    int flags = O_WRONLY | (clear ? O_TRUNC : O_APPEND);
    int fd = open(path, flags);
    if (fd < 0) {
        perror("open");
        chmod(path, 000); // try to restore
        return 8;
    }

    // write message + newline
    if (write_all(fd, msg, strlen(msg)) < 0 || write_all(fd, "\n", 1) < 0) {
        perror("write");
        close(fd);
        chmod(path, 000);
        return 9;
    }

    // restore perms to 000 and close
    if (chmod(path, 000) < 0) perror("chmod 000");
    if (close(fd) < 0) { perror("close"); return 10; }

    return 0;
}
