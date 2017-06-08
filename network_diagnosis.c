
// Copyright 2017 Lawrence Kesteloot
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Performs various network tests in parallel to see what might be going wrong with
// the network.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define MAX_ARGS 128
#define TERMINAL_WIDTH 75

// What kind of test this is.
enum TestType {
    PING,
    DNS,
};

// Information about each test.
struct Test {
    // Type of test.
    enum TestType mTestType;

    // Address (IP for PING, hostname for DNS).
    char *mAddress;

    // PID of spawned task, or 0 if not currently spawned.
    pid_t mPid;

    // Exit code that indicates failure to perform network test.
    int mFailureExitCode;

    // String displaying test history.
    char *mResults;
};

// List of tests to perform.
static struct Test TESTS[] = {
    // Our own routers:
    { PING, "192.168.1.1" },
    { PING, "192.168.1.2" },

    // DNS from Comcast:
    { PING, "75.75.75.75" },
    { PING, "75.75.76.76" },

    // DNS from Google:
    { PING, "8.8.8.8" },
    { PING, "8.8.4.4" },

    // Plunk:
    { PING, "209.123.234.146" },

    // Various DNS lookups using explicit servers.
    { DNS, "75.75.75.75" },
    { DNS, "75.75.76.76" },
    { DNS, "8.8.8.8" },
    { DNS, "8.8.4.4" },
    { DNS, "192.168.1.1" },
};
static int TEST_COUNT = sizeof(TESTS)/sizeof(TESTS[0]);

// Append "more" to "*base", freeing and allocating in-place.
void append(char **base, char *more) {
    int newLength = strlen(*base) + strlen(more);

    char *newBase = (char *) malloc(newLength + 1);
    strcpy(newBase, *base);
    strcat(newBase, more);

    free(*base);
    *base = newBase;
}

// Return the "width" right part of the string.
char *rightString(char *s, int width) {
    int start = (int) strlen(s) - width;
    return &s[start >= 0 ? start : 0];
}

// Get the label for the kind of test.
char *getLabelForType(enum TestType testType) {
    switch (testType) {
        case PING:
            return "Ping";

        case DNS:
            return "DNS";

        default:
            return "Unknown";
    }
}

// Get the widest label we'll have for our tests.
int getMaxWidth(struct Test tests[], int count) {
    int maxWidth = 0;

    for (int i = 0; i < count; i++) {
        struct Test *test = &tests[i];
        char *label = getLabelForType(test->mTestType);

        // Label and address with space in between.
        int width = strlen(label) + 1 + strlen(test->mAddress);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    // Add colon and extra space.
    maxWidth += 2;

    return maxWidth;
}

// Spawn a program. Args are the same as execl().
void spawnCheck(struct Test *test, int failureExitCode, ...) {
    char *args[MAX_ARGS];
    va_list ap;

    // Extract arguments.
    va_start(ap, failureExitCode);
    for (int i = 0; i < MAX_ARGS; i++) {
        args[i] = va_arg(ap, char *);
        if (args[i] == NULL) {
            break;
        }
    }
    va_end(ap);

    // Spawn child.
    pid_t pid = fork();
    if (pid == 0) {
        // Child.

        // Close all file descriptors, we don't want to see anything or have it
        // control our tty input.
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Run program.
        int result = execv(args[0], args);
        if (result == -1) {
            perror("execv");
            exit(1);
        }
    } else {
        // Parent.
        test->mPid = pid;
        test->mFailureExitCode = failureExitCode;
    }
}

// Initialize the Test structures.
void initializeTests(struct Test tests[], int count) {
    for (int i = 0; i < count; i++) {
        struct Test *test = &tests[i];

        test->mPid = 0;
        test->mResults = strdup("");
    }
}

// See if any processes have finished and record their results.
void checkResults(struct Test tests[], int count) {
    // Array of booleans recording which test got a result.
    int *found = (int *) calloc(count, sizeof(int));

    while (1) {
        // See if any children finished.
        int status;
        pid_t pid = wait4(-1, &status, WNOHANG, NULL);
        if (pid == -1) {
            if (errno == ECHILD) {
                // No finished child processes, we're done.
                break;
            } else {
                perror("wait4");
                exit(1);
            }
        }
        if (pid == 0) {
            // No finished child processes, we're done.
            break;
        }

        // Sanity check.
        if (!WIFEXITED(status)) {
            printf("Process did not terminate normally.");
            exit(1);
        }
        status = WEXITSTATUS(status);

        // Find the test that finished.
        for (int i = 0; i < count; i++) {
            struct Test *test = &tests[i];

            if (test->mPid == pid) {
                found[i] = 1;
                test->mPid = 0;

                // Update results.
                char *c = status == 0 ? "*" :
                    status == test->mFailureExitCode ? "X" : "?";
                append(&test->mResults, c);
                break;
            }
        }
    }

    // Write a dot for all the ones that didn't exit this round.
    for (int i = 0; i < count; i++) {
        struct Test *test = &tests[i];

        if (!found[i]) {
            append(&test->mResults, ".");
        }
    }

    free(found);
}

// Spawn new tests.
void spawnTests(struct Test tests[], int count) {
    for (int i = 0; i < count; i++) {
        struct Test *test = &tests[i];

        if (test->mPid == 0) {
            switch (test->mTestType) {
                case PING: {
                    spawnCheck(test,
#if __APPLE__
                            2,
                            "/sbin/ping", "-n", "-c", "1", "-q", "-t", "5",
#elif __linux__
                            1,
                            "/bin/ping", "-n", "-c", "1", "-q", "-W", "5",
#else
#  error "Unknown platform"
#endif
                            test->mAddress,
                            (char *) NULL);
                    break;
                }

                case DNS:
                    spawnCheck(test, 1,
                            "/usr/bin/host", "-t", "a", "plunk.org", test->mAddress,
                            (char *) NULL);
                    break;
            }
        }
    }
}

// Display all tests and their results as a table.
void displayTests(struct Test tests[], int count, int maxWidth) {
    for (int i = 0; i < count; i++) {
        struct Test *test = &tests[i];
        char *label = getLabelForType(test->mTestType);

        int width = printf("%s %s: ", label, test->mAddress);
        for (int j = width; j < maxWidth; j++) {
            putchar(' ');
        }
        puts(rightString(test->mResults, TERMINAL_WIDTH - maxWidth));
    }
}

// Move up "count" rows.
void backupCursor(int count) {
    printf("%c[%dA", 27, count);
}

int main() {
    int maxWidth = getMaxWidth(TESTS, TEST_COUNT);

    initializeTests(TESTS, TEST_COUNT);

    while (1) {
        displayTests(TESTS, TEST_COUNT, maxWidth);
        spawnTests(TESTS, TEST_COUNT);
        sleep(1);
        checkResults(TESTS, TEST_COUNT);
        backupCursor(TEST_COUNT);
    }

    return 0;
}

