#include <windows.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <vector>
#include "../build/compute_spv.h"



static VkResult compute_test(PFN_vkGetDeviceProcAddr get, VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
#define LOAD(name) auto name = (PFN_vk##name)get(device, "vk" #name)
    LOAD(CreateCommandPool); LOAD(DestroyCommandPool); LOAD(AllocateCommandBuffers);
    LOAD(BeginCommandBuffer); LOAD(EndCommandBuffer); LOAD(CmdCopyBuffer); LOAD(CmdPipelineBarrier);
    LOAD(MapMemory); LOAD(UnmapMemory); LOAD(GetDeviceQueue); LOAD(QueueSubmit); LOAD(QueueWaitIdle);
    VkCommandPool pool;
    VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    if (CreateCommandPool(device, &pi, nullptr, &pool)) return VK_ERROR_UNKNOWN;
    VkQueue queue; GetDeviceQueue(device, 0, 0, &queue);
    const uint64_t half = 128ull * 1024 * 1024 + 256;
    const uint64_t sizes[] = {1, 4, 63, 4095, 8191, 8192, 8193, 65520, 65535, 65536, 65552, 131072, 262144, 1048592, 1048593, 134217728};
    VkResult result = VK_SUCCESS;
    for (uint64_t size : sizes) for (uint32_t offset : {0u, 1u, 4u, 16u, 63u}) {
        void *map;
        if (MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &map)) return VK_ERROR_UNKNOWN;
        auto bytes = (unsigned char *)map;
        for (uint64_t i = 0; i < size + 128; i++) bytes[i] = (unsigned char)((i * 131 + (i >> 9)) ^ 0x95);
        memset(bytes + half, 0xa6, size + 128);
        UnmapMemory(device, memory);
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = pool; ai.commandBufferCount = 1;
        VkCommandBuffer cmd;
        if (AllocateCommandBuffers(device, &ai, &cmd)) return VK_ERROR_UNKNOWN;
        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        BeginCommandBuffer(cmd, &begin);
        VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        VkBufferCopy region = {offset, half + offset, size};
        CmdCopyBuffer(cmd, buffer, buffer, 1, &region);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        CmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        EndCommandBuffer(cmd);
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        result = QueueSubmit(queue,1,&submit,VK_NULL_HANDLE);
        if (result) return result;
        result = QueueWaitIdle(queue);
        if (result) return result;
        result = MapMemory(device,memory,0,VK_WHOLE_SIZE,0,&map);
        if (result) return result;
        bytes=(unsigned char *)map;
        for (uint64_t i=0;i<size+128;i++) {
            unsigned char expected = i >= offset && i < size+offset ? bytes[i] : 0xa6;
            if (bytes[half+i] != expected) {
                printf("FAIL size=%llu offset=%u byte=%llu got=%u expected=%u\n",size,offset,i,bytes[half+i],expected);
                result = VK_ERROR_UNKNOWN; break;
            }
        }
        UnmapMemory(device,memory);
        if (result) return result;
        printf("copy size=%llu offset=%u PASS\n",size,offset);
    }
    DestroyCommandPool(device,pool,nullptr);
    return result;
#undef LOAD
}

static VkResult device_memory_test(PFN_vkGetInstanceProcAddr get, VkInstance instance, VkPhysicalDevice physical)
{
    auto create = (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
    auto gdpa = (PFN_vkGetDeviceProcAddr)get(instance, "vkGetDeviceProcAddr");
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    VkDeviceCreateInfo info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    VkDevice device = VK_NULL_HANDLE;
    VkResult result = create(physical, &info, nullptr, &device);
    std::printf("vkCreateDevice: %d\n", result);
    if (result != VK_SUCCESS) return result;
#define LOAD(name) auto name = (PFN_vk##name)gdpa(device, "vk" #name)
    LOAD(DestroyDevice); LOAD(CreateBuffer); LOAD(DestroyBuffer);
    LOAD(GetBufferMemoryRequirements); LOAD(AllocateMemory); LOAD(FreeMemory);
    LOAD(BindBufferMemory); LOAD(MapMemory); LOAD(UnmapMemory);
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = 268435968;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    result = CreateBuffer(device, &bi, nullptr, &buffer);
    if (result == VK_SUCCESS) {
        VkMemoryRequirements requirements = {};
        GetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = 1;
        result = AllocateMemory(device, &allocation, nullptr, &memory);
        std::printf("vkAllocateMemory: %d size=%llu\n", result, requirements.size);
    }
    if (result == VK_SUCCESS) result = BindBufferMemory(device, buffer, memory, 0);
    if (result == VK_SUCCESS) {
        void *mapping = nullptr;
        result = MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapping);
        if (result == VK_SUCCESS) {
            auto words = (volatile uint32_t *)mapping;
            for (uint32_t i = 0; i < 1024; i++) words[i] = i ^ 0xdeadbeef;
            MemoryBarrier();
            for (uint32_t i = 0; i < 1024; i++)
                if (words[i] != (i ^ 0xdeadbeef)) result = VK_ERROR_UNKNOWN;
            UnmapMemory(device, memory);
        }
        std::printf("buffer CPU mapping/readback: %d\n", result);
    }

    if (result == VK_SUCCESS) result = compute_test(gdpa, device, buffer, memory);
    if (buffer) DestroyBuffer(device, buffer, nullptr);
    if (memory) FreeMemory(device, memory, nullptr);
    DestroyDevice(device, nullptr);
    std::printf("vkDestroyDevice complete\n");
    return result;
#undef LOAD
}

int main(int argc, char **argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    auto get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vk_icdGetInstanceProcAddr"));
    if (!get) get = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(module, "vkGetInstanceProcAddr"));
    if (!get) {
        std::printf("ICD entrypoint missing: %lu\n", GetLastError());
        return 1;
    }
    auto create = reinterpret_cast<PFN_vkCreateInstance>(get(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create) return 1;
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Turnip WDDM smoke";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create(&info, nullptr, &instance);
    std::printf("vkCreateInstance: %d\n", result);
    if (result != VK_SUCCESS) return 1;
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get(instance, "vkEnumeratePhysicalDevices"));
    auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(get(instance, "vkDestroyInstance"));
    uint32_t count = 0;
    result = enumerate ? enumerate(instance, &count, nullptr) : VK_ERROR_INITIALIZATION_FAILED;
    std::printf("vkEnumeratePhysicalDevices: %d count=%u\n", result, count);
    if (result == VK_SUCCESS && count) {
        std::vector<VkPhysicalDevice> devices(count);
        result = enumerate(instance, &count, devices.data());
        auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(get(instance, "vkGetPhysicalDeviceProperties"));
        auto memory = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(get(instance, "vkGetPhysicalDeviceMemoryProperties"));
        if (!properties || !memory) result = VK_ERROR_INITIALIZATION_FAILED;
        for (uint32_t i = 0; result == VK_SUCCESS && i < count; ++i) {
            VkPhysicalDeviceProperties p = {};
            VkPhysicalDeviceMemoryProperties m = {};
            properties(devices[i], &p);
            memory(devices[i], &m);
            std::printf("device=%s vendor=0x%x device=0x%x API=%u.%u.%u heaps=%u types=%u\n",
                p.deviceName, p.vendorID, p.deviceID, VK_VERSION_MAJOR(p.apiVersion),
                VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion),
                m.memoryHeapCount, m.memoryTypeCount);
            result = device_memory_test(get, instance, devices[i]);
        }
    }
    if (destroy) destroy(instance, nullptr);
    FreeLibrary(module);
    return result == VK_SUCCESS ? 0 : 1;
}
