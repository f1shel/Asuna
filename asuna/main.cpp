#include "tracer/tracer.h"

int main() {
    Tracer asuna;
    asuna.init();
    asuna.run();
    asuna.deinit();
    return 0;
}