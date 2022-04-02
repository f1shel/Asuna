#include "tracer/asuna_tracer.h"
#include <chrono>

int main() {
    AsunaTracer asuna;
    asuna.init();

    // Main loop
    while (!asuna.glfwShouldClose()) {
        asuna.glfwPoll();
        if (asuna.isMinimized()) continue;
    }

    asuna.destroy();
    return 0;
}