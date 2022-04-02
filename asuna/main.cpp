#include <nvvk/context_vk.hpp>

#include <iostream>
using namespace std;
int main() {
    cout << "Hello, Asuna!" << endl;

    // Create the Vulkan context, consisting of an instance, device, physical device, and queues.
    nvvk::ContextCreateInfo deviceInfo;  // One can modify this to load different extensions or pick the Vulkan core version
    nvvk::Context           context;     // Encapsulates device state in a single object
    context.init(deviceInfo);            // Initialize the context
    context.deinit();                    // Don't forget to clean up at the end of the program!

    return 0;
}