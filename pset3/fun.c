#include "io61.h"

int main(int argc, char const *argv[]) {
    io61_file* stdout = io61_fdopen(1,O_WRONLY);
    char msg[] = "Hello World!\n";
    io61_write(stdout, msg, sizeof(msg));
    io61_close(stdout);
    return 0;
}
