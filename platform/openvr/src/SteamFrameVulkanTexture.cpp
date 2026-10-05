#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "SteamFrameVulkanTexture.h"

#include <dlfcn.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace phonecast::platform::openvr {
namespace vr = ::vr;
namespace {

constexpr VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr std::uint64_t kUploadTimeoutNanoseconds = 1'000'000'000ULL;

std::string VulkanError(const char* operation, VkResult result) {
    return std::string(operation) + " failed (VkResult " +
           std::to_string(static_cast<int>(result)) + ").";
}

std::vector<std::string> SplitExtensionNames(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string name;
    while (input >> name) result.push_back(name);
    return result;
}

std::vector<const char*> ExtensionPointers(const std::vector<std::string>& names) {
    std::vector<const char*> result;
    result.reserve(names.size());
    for (const auto& name : names) result.push_back(name.c_str());
    return result;
}

bool ValidateExtensions(const std::vector<std::string>& required,
                        const std::vector<VkExtensionProperties>& available,
                        const char* kind, std::string& error) {
    for (const auto& name : required) {
        const auto found = std::find_if(
            available.begin(), available.end(), [&name](const VkExtensionProperties& extension) {
                return name == extension.extensionName;
            });
        if (found == available.end()) {
            error = std::string("OpenVR requires unavailable Vulkan ") + kind +
                    " extension " + name + ".";
            return false;
        }
    }
    return true;
}

std::string RequiredInstanceExtensions(vr::IVRCompositor* compositor) {
    const std::uint32_t size = compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
    if (size == 0) return {};
    std::vector<char> text(size, '\0');
    compositor->GetVulkanInstanceExtensionsRequired(text.data(), size);
    return text.data();
}

std::string RequiredDeviceExtensions(vr::IVRCompositor* compositor,
                                     VkPhysicalDevice physicalDevice) {
    auto* openVrDevice = reinterpret_cast<VkPhysicalDevice_T*>(physicalDevice);
    const std::uint32_t size =
        compositor->GetVulkanDeviceExtensionsRequired(openVrDevice, nullptr, 0);
    if (size == 0) return {};
    std::vector<char> text(size, '\0');
    compositor->GetVulkanDeviceExtensionsRequired(openVrDevice, text.data(), size);
    return text.data();
}

}  // namespace

class SteamFrameVulkanTexture::Impl {
public:
    explicit Impl(core::ILogger& logger) : logger(logger) {}

    template <typename Function>
    bool LoadGlobal(Function& function, const char* name, std::string& error) {
        function = reinterpret_cast<Function>(getInstanceProcAddr(VK_NULL_HANDLE, name));
        if (function != nullptr) return true;
        error = std::string("Vulkan loader does not provide ") + name + ".";
        return false;
    }

    template <typename Function>
    bool LoadInstance(Function& function, const char* name, std::string& error) {
        function = reinterpret_cast<Function>(getInstanceProcAddr(instance, name));
        if (function != nullptr) return true;
        error = std::string("Vulkan instance does not provide ") + name + ".";
        return false;
    }

    template <typename Function>
    bool LoadDevice(Function& function, const char* name, std::string& error) {
        function = reinterpret_cast<Function>(getDeviceProcAddr(device, name));
        if (function != nullptr) return true;
        error = std::string("Vulkan device does not provide ") + name + ".";
        return false;
    }

    bool LoadLibrary(std::string& error) {
        library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (library == nullptr) {
            const char* detail = dlerror();
            error = std::string("Could not load libvulkan.so.1") +
                    (detail != nullptr ? std::string(": ") + detail : ".");
            return false;
        }
        getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(library, "vkGetInstanceProcAddr"));
        if (getInstanceProcAddr == nullptr) {
            error = "Vulkan loader does not export vkGetInstanceProcAddr.";
            return false;
        }
        return LoadGlobal(createInstance, "vkCreateInstance", error) &&
               LoadGlobal(enumerateInstanceExtensionProperties,
                          "vkEnumerateInstanceExtensionProperties", error);
    }

    bool LoadInstanceFunctions(std::string& error) {
        return LoadInstance(destroyInstance, "vkDestroyInstance", error) &&
               LoadInstance(enumeratePhysicalDevices, "vkEnumeratePhysicalDevices", error) &&
               LoadInstance(getPhysicalDeviceQueueFamilyProperties,
                            "vkGetPhysicalDeviceQueueFamilyProperties", error) &&
               LoadInstance(enumerateDeviceExtensionProperties,
                            "vkEnumerateDeviceExtensionProperties", error) &&
               LoadInstance(getPhysicalDeviceMemoryProperties,
                            "vkGetPhysicalDeviceMemoryProperties", error) &&
               LoadInstance(getPhysicalDeviceProperties,
                            "vkGetPhysicalDeviceProperties", error) &&
               LoadInstance(createDevice, "vkCreateDevice", error) &&
               LoadInstance(getDeviceProcAddr, "vkGetDeviceProcAddr", error);
    }

    bool LoadDeviceFunctions(std::string& error) {
        return LoadDevice(destroyDevice, "vkDestroyDevice", error) &&
               LoadDevice(getDeviceQueue, "vkGetDeviceQueue", error) &&
               LoadDevice(createCommandPool, "vkCreateCommandPool", error) &&
               LoadDevice(destroyCommandPool, "vkDestroyCommandPool", error) &&
               LoadDevice(deviceWaitIdle, "vkDeviceWaitIdle", error) &&
               LoadDevice(createImage, "vkCreateImage", error) &&
               LoadDevice(destroyImage, "vkDestroyImage", error) &&
               LoadDevice(getImageMemoryRequirements, "vkGetImageMemoryRequirements", error) &&
               LoadDevice(allocateMemory, "vkAllocateMemory", error) &&
               LoadDevice(freeMemory, "vkFreeMemory", error) &&
               LoadDevice(bindImageMemory, "vkBindImageMemory", error) &&
               LoadDevice(createBuffer, "vkCreateBuffer", error) &&
               LoadDevice(destroyBuffer, "vkDestroyBuffer", error) &&
               LoadDevice(getBufferMemoryRequirements, "vkGetBufferMemoryRequirements", error) &&
               LoadDevice(bindBufferMemory, "vkBindBufferMemory", error) &&
               LoadDevice(mapMemory, "vkMapMemory", error) &&
               LoadDevice(unmapMemory, "vkUnmapMemory", error) &&
               LoadDevice(allocateCommandBuffers, "vkAllocateCommandBuffers", error) &&
               LoadDevice(freeCommandBuffers, "vkFreeCommandBuffers", error) &&
               LoadDevice(createFence, "vkCreateFence", error) &&
               LoadDevice(destroyFence, "vkDestroyFence", error) &&
               LoadDevice(resetCommandBuffer, "vkResetCommandBuffer", error) &&
               LoadDevice(beginCommandBuffer, "vkBeginCommandBuffer", error) &&
               LoadDevice(cmdPipelineBarrier, "vkCmdPipelineBarrier", error) &&
               LoadDevice(cmdCopyBufferToImage, "vkCmdCopyBufferToImage", error) &&
               LoadDevice(endCommandBuffer, "vkEndCommandBuffer", error) &&
               LoadDevice(resetFences, "vkResetFences", error) &&
               LoadDevice(queueSubmit, "vkQueueSubmit", error) &&
               LoadDevice(waitForFences, "vkWaitForFences", error);
    }

    bool Initialize(vr::IVRSystem* system, vr::IVRCompositor* compositor,
                    std::string& error) {
        Shutdown();
        if (system == nullptr || compositor == nullptr) {
            error = "OpenVR system or compositor is unavailable for Vulkan initialization.";
            return false;
        }
        if (!LoadLibrary(error)) {
            Shutdown();
            return false;
        }

        const auto instanceExtensionNames =
            SplitExtensionNames(RequiredInstanceExtensions(compositor));
        std::uint32_t extensionCount = 0;
        VkResult result = enumerateInstanceExtensionProperties(
            nullptr, &extensionCount, nullptr);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkEnumerateInstanceExtensionProperties", result);
            Shutdown();
            return false;
        }
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        result = enumerateInstanceExtensionProperties(
            nullptr, &extensionCount, availableExtensions.data());
        if (result != VK_SUCCESS ||
            !ValidateExtensions(instanceExtensionNames, availableExtensions,
                                "instance", error)) {
            if (result != VK_SUCCESS)
                error = VulkanError("vkEnumerateInstanceExtensionProperties", result);
            Shutdown();
            return false;
        }
        const auto instanceExtensionPointers = ExtensionPointers(instanceExtensionNames);
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "PhoneCast VR";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        applicationInfo.pEngineName = "PhoneCast";
        applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        applicationInfo.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &applicationInfo;
        instanceInfo.enabledExtensionCount =
            static_cast<std::uint32_t>(instanceExtensionPointers.size());
        instanceInfo.ppEnabledExtensionNames = instanceExtensionPointers.data();
        result = createInstance(&instanceInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkCreateInstance", result);
            instance = VK_NULL_HANDLE;
            Shutdown();
            return false;
        }
        if (!LoadInstanceFunctions(error)) {
            Shutdown();
            return false;
        }

        std::uint64_t compositorDevice = 0;
        system->GetOutputDevice(&compositorDevice, vr::TextureType_Vulkan,
                                reinterpret_cast<VkInstance_T*>(instance));
        physicalDevice = reinterpret_cast<VkPhysicalDevice>(compositorDevice);
        if (physicalDevice == VK_NULL_HANDLE) {
            error = "OpenVR did not identify its Vulkan compositor device.";
            Shutdown();
            return false;
        }

        std::uint32_t familyCount = 0;
        getPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        getPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
        const auto family = std::find_if(
            families.begin(), families.end(), [](const VkQueueFamilyProperties& properties) {
                return properties.queueCount > 0 &&
                       (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            });
        if (family == families.end()) {
            error = "The OpenVR Vulkan device has no graphics-capable queue family.";
            Shutdown();
            return false;
        }
        queueFamilyIndex = static_cast<std::uint32_t>(family - families.begin());

        const auto deviceExtensionNames = SplitExtensionNames(
            RequiredDeviceExtensions(compositor, physicalDevice));
        extensionCount = 0;
        result = enumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &extensionCount, nullptr);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkEnumerateDeviceExtensionProperties", result);
            Shutdown();
            return false;
        }
        availableExtensions.assign(extensionCount, {});
        result = enumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &extensionCount, availableExtensions.data());
        if (result != VK_SUCCESS ||
            !ValidateExtensions(deviceExtensionNames, availableExtensions,
                                "device", error)) {
            if (result != VK_SUCCESS)
                error = VulkanError("vkEnumerateDeviceExtensionProperties", result);
            Shutdown();
            return false;
        }
        const auto deviceExtensionPointers = ExtensionPointers(deviceExtensionNames);
        const float queuePriority = 1.0F;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount =
            static_cast<std::uint32_t>(deviceExtensionPointers.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensionPointers.data();
        result = createDevice(physicalDevice, &deviceInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkCreateDevice", result);
            device = VK_NULL_HANDLE;
            Shutdown();
            return false;
        }
        if (!LoadDeviceFunctions(error)) {
            Shutdown();
            return false;
        }
        getDeviceQueue(device, queueFamilyIndex, 0, &queue);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        result = createCommandPool(device, &poolInfo, nullptr, &commandPool);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkCreateCommandPool", result);
            commandPool = VK_NULL_HANDLE;
            Shutdown();
            return false;
        }

        VkPhysicalDeviceProperties properties{};
        getPhysicalDeviceProperties(physicalDevice, &properties);
        logger.Log(core::LogLevel::Info, "openvr-vulkan",
                   std::string("Initialized reusable Vulkan overlay textures on ") +
                   properties.deviceName + " with " +
                   std::to_string(instanceExtensionNames.size()) + " instance and " +
                   std::to_string(deviceExtensionNames.size()) + " device extensions.");
        error.clear();
        return true;
    }

    std::uint32_t FindMemoryType(std::uint32_t allowed,
                                 VkMemoryPropertyFlags required) const {
        VkPhysicalDeviceMemoryProperties properties{};
        getPhysicalDeviceMemoryProperties(physicalDevice, &properties);
        for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
            if ((allowed & (1U << index)) != 0 &&
                (properties.memoryTypes[index].propertyFlags & required) == required)
                return index;
        }
        return std::numeric_limits<std::uint32_t>::max();
    }

    bool CreateTextureResources(std::uint32_t newWidth, std::uint32_t newHeight,
                                std::string& error) {
        DestroyTextureResources();
        width = newWidth;
        height = newHeight;
        const VkDeviceSize byteCount =
            static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4U;

        for (std::size_t index = 0; index < images.size(); ++index) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = kTextureFormat;
            imageInfo.extent = {width, height, 1};
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkResult result = createImage(device, &imageInfo, nullptr, &images[index]);
            if (result != VK_SUCCESS) {
                error = VulkanError("vkCreateImage", result);
                DestroyTextureResources();
                return false;
            }
            VkMemoryRequirements requirements{};
            getImageMemoryRequirements(device, images[index], &requirements);
            const std::uint32_t memoryType = FindMemoryType(
                requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (memoryType == std::numeric_limits<std::uint32_t>::max()) {
                error = "No device-local Vulkan memory type is available for the overlay image.";
                DestroyTextureResources();
                return false;
            }
            VkMemoryAllocateInfo allocation{};
            allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = memoryType;
            result = allocateMemory(device, &allocation, nullptr, &imageMemory[index]);
            if (result != VK_SUCCESS) {
                error = VulkanError("vkAllocateMemory for overlay image", result);
                DestroyTextureResources();
                return false;
            }
            result = bindImageMemory(device, images[index], imageMemory[index], 0);
            if (result != VK_SUCCESS) {
                error = VulkanError("vkBindImageMemory", result);
                DestroyTextureResources();
                return false;
            }
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = byteCount;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = createBuffer(device, &bufferInfo, nullptr, &stagingBuffer);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkCreateBuffer", result);
            DestroyTextureResources();
            return false;
        }
        VkMemoryRequirements requirements{};
        getBufferMemoryRequirements(device, stagingBuffer, &requirements);
        const std::uint32_t memoryType = FindMemoryType(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memoryType == std::numeric_limits<std::uint32_t>::max()) {
            error = "No host-visible coherent Vulkan memory type is available for frame upload.";
            DestroyTextureResources();
            return false;
        }
        VkMemoryAllocateInfo allocation{};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memoryType;
        result = allocateMemory(device, &allocation, nullptr, &stagingMemory);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkAllocateMemory for staging buffer", result);
            DestroyTextureResources();
            return false;
        }
        result = bindBufferMemory(device, stagingBuffer, stagingMemory, 0);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkBindBufferMemory", result);
            DestroyTextureResources();
            return false;
        }
        result = mapMemory(device, stagingMemory, 0, byteCount, 0, &mappedStaging);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkMapMemory", result);
            mappedStaging = nullptr;
            DestroyTextureResources();
            return false;
        }

        VkCommandBufferAllocateInfo commandInfo{};
        commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandInfo.commandPool = commandPool;
        commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandInfo.commandBufferCount = 1;
        result = allocateCommandBuffers(device, &commandInfo, &commandBuffer);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkAllocateCommandBuffers", result);
            commandBuffer = VK_NULL_HANDLE;
            DestroyTextureResources();
            return false;
        }
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        result = createFence(device, &fenceInfo, nullptr, &uploadFence);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkCreateFence", result);
            uploadFence = VK_NULL_HANDLE;
            DestroyTextureResources();
            return false;
        }
        imageUsed.fill(false);
        nextImage = 0;
        logger.Log(core::LogLevel::Info, "openvr-vulkan",
                   "Created reusable double-buffered RGBA texture " +
                   std::to_string(width) + "x" + std::to_string(height) + ".");
        return true;
    }

    void DestroyTextureResources() noexcept {
        if (device == VK_NULL_HANDLE) return;
        if (deviceWaitIdle != nullptr) deviceWaitIdle(device);
        if (uploadFence != VK_NULL_HANDLE && destroyFence != nullptr)
            destroyFence(device, uploadFence, nullptr);
        if (commandBuffer != VK_NULL_HANDLE && freeCommandBuffers != nullptr &&
            commandPool != VK_NULL_HANDLE)
            freeCommandBuffers(device, commandPool, 1, &commandBuffer);
        if (mappedStaging != nullptr && unmapMemory != nullptr)
            unmapMemory(device, stagingMemory);
        if (stagingBuffer != VK_NULL_HANDLE && destroyBuffer != nullptr)
            destroyBuffer(device, stagingBuffer, nullptr);
        if (stagingMemory != VK_NULL_HANDLE && freeMemory != nullptr)
            freeMemory(device, stagingMemory, nullptr);
        for (std::size_t index = 0; index < images.size(); ++index) {
            if (images[index] != VK_NULL_HANDLE && destroyImage != nullptr)
                destroyImage(device, images[index], nullptr);
            if (imageMemory[index] != VK_NULL_HANDLE && freeMemory != nullptr)
                freeMemory(device, imageMemory[index], nullptr);
        }
        images.fill(VK_NULL_HANDLE);
        imageMemory.fill(VK_NULL_HANDLE);
        imageUsed.fill(false);
        uploadFence = VK_NULL_HANDLE;
        commandBuffer = VK_NULL_HANDLE;
        mappedStaging = nullptr;
        stagingBuffer = VK_NULL_HANDLE;
        stagingMemory = VK_NULL_HANDLE;
        width = 0;
        height = 0;
        nextImage = 0;
    }

    bool Update(vr::IVROverlay* overlayApi, vr::VROverlayHandle_t overlay,
                const std::uint8_t* rgba, std::uint32_t newWidth,
                std::uint32_t newHeight, std::string& error) {
        if (device == VK_NULL_HANDLE || overlayApi == nullptr || rgba == nullptr) {
            error = "Vulkan overlay texture is not initialized.";
            return false;
        }
        if (newWidth == 0 || newHeight == 0) {
            error = "Vulkan overlay texture dimensions must be non-zero.";
            return false;
        }
        if (width != newWidth || height != newHeight) {
            if (width != 0 && height != 0) {
                const auto clearResult = overlayApi->ClearOverlayTexture(overlay);
                if (clearResult != vr::VROverlayError_None) {
                    const char* name = overlayApi->GetOverlayErrorNameFromEnum(clearResult);
                    error = std::string("ClearOverlayTexture before resize failed: ") +
                            (name != nullptr ? name : "unknown error") + ".";
                    return false;
                }
                // OpenVR has no compositor-release fence for overlay textures.
                // Give several compositor frames to retire the old image before
                // freeing its Vulkan memory; this runs only on orientation/size
                // changes, never in the steady 30 FPS upload path.
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (!CreateTextureResources(newWidth, newHeight, error)) return false;
        }

        const std::size_t index = nextImage;
        nextImage = (nextImage + 1U) % images.size();
        std::memcpy(mappedStaging, rgba,
                    static_cast<std::size_t>(width) * height * 4U);

        VkResult result = resetCommandBuffer(commandBuffer, 0);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkResetCommandBuffer", result);
            return false;
        }
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = beginCommandBuffer(commandBuffer, &beginInfo);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkBeginCommandBuffer", result);
            return false;
        }
        VkImageMemoryBarrier toDestination{};
        toDestination.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toDestination.srcAccessMask = imageUsed[index] ? VK_ACCESS_TRANSFER_READ_BIT : 0;
        toDestination.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toDestination.oldLayout = imageUsed[index]
            ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        toDestination.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDestination.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDestination.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDestination.image = images[index];
        toDestination.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &toDestination);

        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        cmdCopyBufferToImage(commandBuffer, stagingBuffer, images[index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        VkImageMemoryBarrier toSource = toDestination;
        toSource.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toSource.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT |
                                 VK_ACCESS_SHADER_READ_BIT;
        toSource.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSource.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &toSource);
        result = endCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS) {
            error = VulkanError("vkEndCommandBuffer", result);
            return false;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        result = resetFences(device, 1, &uploadFence);
        if (result == VK_SUCCESS)
            result = queueSubmit(queue, 1, &submitInfo, uploadFence);
        if (result == VK_SUCCESS)
            result = waitForFences(device, 1, &uploadFence, VK_TRUE,
                                   kUploadTimeoutNanoseconds);
        if (result != VK_SUCCESS) {
            error = VulkanError("Vulkan frame upload", result);
            return false;
        }
        imageUsed[index] = true;

        vr::VRVulkanTextureData_t textureData{};
        textureData.m_nImage = reinterpret_cast<std::uint64_t>(images[index]);
        textureData.m_pDevice = reinterpret_cast<VkDevice_T*>(device);
        textureData.m_pPhysicalDevice =
            reinterpret_cast<VkPhysicalDevice_T*>(physicalDevice);
        textureData.m_pInstance = reinterpret_cast<VkInstance_T*>(instance);
        textureData.m_pQueue = reinterpret_cast<VkQueue_T*>(queue);
        textureData.m_nQueueFamilyIndex = queueFamilyIndex;
        textureData.m_nWidth = width;
        textureData.m_nHeight = height;
        textureData.m_nFormat = kTextureFormat;
        textureData.m_nSampleCount = 1;
        vr::Texture_t texture{&textureData, vr::TextureType_Vulkan,
                              vr::ColorSpace_Gamma};
        const auto overlayResult = overlayApi->SetOverlayTexture(overlay, &texture);
        if (overlayResult != vr::VROverlayError_None) {
            const char* name = overlayApi->GetOverlayErrorNameFromEnum(overlayResult);
            error = std::string("SetOverlayTexture failed: ") +
                    (name != nullptr ? name : "unknown error") + " (" +
                    std::to_string(static_cast<int>(overlayResult)) + ").";
            return false;
        }
        error.clear();
        return true;
    }

    void Shutdown() noexcept {
        DestroyTextureResources();
        if (device != VK_NULL_HANDLE) {
            if (deviceWaitIdle != nullptr) deviceWaitIdle(device);
            if (commandPool != VK_NULL_HANDLE && destroyCommandPool != nullptr)
                destroyCommandPool(device, commandPool, nullptr);
            if (destroyDevice != nullptr) destroyDevice(device, nullptr);
        }
        if (instance != VK_NULL_HANDLE && destroyInstance != nullptr)
            destroyInstance(instance, nullptr);
        if (library != nullptr) dlclose(library);

        library = nullptr;
        instance = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        queue = VK_NULL_HANDLE;
        commandPool = VK_NULL_HANDLE;
        queueFamilyIndex = 0;
        getInstanceProcAddr = nullptr;
        getDeviceProcAddr = nullptr;
        createInstance = nullptr;
        enumerateInstanceExtensionProperties = nullptr;
        destroyInstance = nullptr;
        enumeratePhysicalDevices = nullptr;
        getPhysicalDeviceQueueFamilyProperties = nullptr;
        enumerateDeviceExtensionProperties = nullptr;
        getPhysicalDeviceMemoryProperties = nullptr;
        getPhysicalDeviceProperties = nullptr;
        createDevice = nullptr;
        destroyDevice = nullptr;
        getDeviceQueue = nullptr;
        createCommandPool = nullptr;
        destroyCommandPool = nullptr;
        deviceWaitIdle = nullptr;
        createImage = nullptr;
        destroyImage = nullptr;
        getImageMemoryRequirements = nullptr;
        allocateMemory = nullptr;
        freeMemory = nullptr;
        bindImageMemory = nullptr;
        createBuffer = nullptr;
        destroyBuffer = nullptr;
        getBufferMemoryRequirements = nullptr;
        bindBufferMemory = nullptr;
        mapMemory = nullptr;
        unmapMemory = nullptr;
        allocateCommandBuffers = nullptr;
        freeCommandBuffers = nullptr;
        createFence = nullptr;
        destroyFence = nullptr;
        resetCommandBuffer = nullptr;
        beginCommandBuffer = nullptr;
        cmdPipelineBarrier = nullptr;
        cmdCopyBufferToImage = nullptr;
        endCommandBuffer = nullptr;
        resetFences = nullptr;
        queueSubmit = nullptr;
        waitForFences = nullptr;
    }

    core::ILogger& logger;
    void* library{nullptr};
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkQueue queue{VK_NULL_HANDLE};
    VkCommandPool commandPool{VK_NULL_HANDLE};
    std::uint32_t queueFamilyIndex{};

    std::array<VkImage, 2> images{VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<VkDeviceMemory, 2> imageMemory{VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<bool, 2> imageUsed{false, false};
    VkBuffer stagingBuffer{VK_NULL_HANDLE};
    VkDeviceMemory stagingMemory{VK_NULL_HANDLE};
    void* mappedStaging{nullptr};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence uploadFence{VK_NULL_HANDLE};
    std::uint32_t width{};
    std::uint32_t height{};
    std::size_t nextImage{};

    PFN_vkGetInstanceProcAddr getInstanceProcAddr{};
    PFN_vkGetDeviceProcAddr getDeviceProcAddr{};
    PFN_vkCreateInstance createInstance{};
    PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties{};
    PFN_vkDestroyInstance destroyInstance{};
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices{};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getPhysicalDeviceQueueFamilyProperties{};
    PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties{};
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties{};
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties{};
    PFN_vkCreateDevice createDevice{};
    PFN_vkDestroyDevice destroyDevice{};
    PFN_vkGetDeviceQueue getDeviceQueue{};
    PFN_vkCreateCommandPool createCommandPool{};
    PFN_vkDestroyCommandPool destroyCommandPool{};
    PFN_vkDeviceWaitIdle deviceWaitIdle{};
    PFN_vkCreateImage createImage{};
    PFN_vkDestroyImage destroyImage{};
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements{};
    PFN_vkAllocateMemory allocateMemory{};
    PFN_vkFreeMemory freeMemory{};
    PFN_vkBindImageMemory bindImageMemory{};
    PFN_vkCreateBuffer createBuffer{};
    PFN_vkDestroyBuffer destroyBuffer{};
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements{};
    PFN_vkBindBufferMemory bindBufferMemory{};
    PFN_vkMapMemory mapMemory{};
    PFN_vkUnmapMemory unmapMemory{};
    PFN_vkAllocateCommandBuffers allocateCommandBuffers{};
    PFN_vkFreeCommandBuffers freeCommandBuffers{};
    PFN_vkCreateFence createFence{};
    PFN_vkDestroyFence destroyFence{};
    PFN_vkResetCommandBuffer resetCommandBuffer{};
    PFN_vkBeginCommandBuffer beginCommandBuffer{};
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier{};
    PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage{};
    PFN_vkEndCommandBuffer endCommandBuffer{};
    PFN_vkResetFences resetFences{};
    PFN_vkQueueSubmit queueSubmit{};
    PFN_vkWaitForFences waitForFences{};
};

SteamFrameVulkanTexture::SteamFrameVulkanTexture(core::ILogger& logger)
    : impl_(std::make_unique<Impl>(logger)) {}

SteamFrameVulkanTexture::~SteamFrameVulkanTexture() { Shutdown(); }

bool SteamFrameVulkanTexture::Initialize(vr::IVRSystem* system,
                                         vr::IVRCompositor* compositor,
                                         std::string& error) {
    return impl_->Initialize(system, compositor, error);
}

bool SteamFrameVulkanTexture::Update(vr::IVROverlay* overlayApi,
                                     vr::VROverlayHandle_t overlay,
                                     const std::uint8_t* rgba,
                                     std::uint32_t width,
                                     std::uint32_t height,
                                     std::string& error) {
    return impl_->Update(overlayApi, overlay, rgba, width, height, error);
}

void SteamFrameVulkanTexture::Shutdown() noexcept { impl_->Shutdown(); }

}  // namespace phonecast::platform::openvr
