#include <vulkan/vulkan.h>

// Global checker declared elsewhere
void check_vk_result(VkResult err);

namespace AlgeUI {
// Provide a namespaced shim to satisfy references built with AlgeUI::check_vk_result
void check_vk_result(VkResult err) { ::check_vk_result(err); }
}
