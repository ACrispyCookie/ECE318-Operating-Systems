#include <sys/syscall.h>
#include <unistd.h>
#include "syscall_wrapper.h"

int hello_syscall_wrapper(void) {
    return syscall(__NR_hello_syscall);
}
