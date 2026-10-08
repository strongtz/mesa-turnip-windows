#include <windows.h>
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../build/typed_spv.h"



static VkResult compute_test(PFN_vkGetDeviceProcAddr get, VkDevice device, VkBuffer buffer, VkDeviceMemory memory)
{
#define LOAD(name) auto name = (PFN_vk##name)get(device, "vk" #name)
    LOAD(CreateDescriptorSetLayout); LOAD(DestroyDescriptorSetLayout);
    LOAD(CreatePipelineLayout); LOAD(DestroyPipelineLayout);
    LOAD(CreateShaderModule); LOAD(DestroyShaderModule);
    LOAD(CreateComputePipelines); LOAD(DestroyPipeline);
    LOAD(CreateDescriptorPool); LOAD(DestroyDescriptorPool);
    LOAD(AllocateDescriptorSets); LOAD(UpdateDescriptorSets);
    LOAD(CreateCommandPool); LOAD(DestroyCommandPool); LOAD(AllocateCommandBuffers);
    LOAD(BeginCommandBuffer); LOAD(EndCommandBuffer); LOAD(CmdPipelineBarrier);
    LOAD(CmdBindPipeline); LOAD(CmdBindDescriptorSets); LOAD(CmdDispatch);
    LOAD(CreateFence); LOAD(DestroyFence); LOAD(GetDeviceQueue); LOAD(QueueSubmit); LOAD(WaitForFences);
    LOAD(MapMemory); LOAD(UnmapMemory);
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule shader = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptors = VK_NULL_HANDLE;
    VkCommandPool commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    do {
        void *input = nullptr;
        result = MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &input);
        if (result) break;
        for (uint32_t i = 0; i < 1024; i++)
            ((uint32_t *)input)[i * 8] = 0x1234abcdu ^ (i * 7919u);
        UnmapMemory(device, memory);
        VkDescriptorSetLayoutBinding binding = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo si = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        si.bindingCount = 1; si.pBindings = &binding;
        result = CreateDescriptorSetLayout(device, &si, nullptr, &set_layout);
        if (result) break;
        VkPipelineLayoutCreateInfo li = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        li.setLayoutCount = 1; li.pSetLayouts = &set_layout;
        result = CreatePipelineLayout(device, &li, nullptr, &layout);
        if (result) break;
        VkShaderModuleCreateInfo mi = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        mi.codeSize = sizeof(typed_spv); mi.pCode = typed_spv;
        result = CreateShaderModule(device, &mi, nullptr, &shader);
        if (result) break;
        VkComputePipelineCreateInfo pi = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pi.layout = layout;
        pi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pi.stage.module = shader; pi.stage.pName = "main";
        result = CreateComputePipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &pipeline);
        std::printf("vkCreateComputePipelines: %d\n", result);
        if (result) break;
        VkDescriptorPoolSize size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
        VkDescriptorPoolCreateInfo di = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        di.maxSets = 1; di.poolSizeCount = 1; di.pPoolSizes = &size;
        result = CreateDescriptorPool(device, &di, nullptr, &descriptors);
        if (result) break;
        VkDescriptorSetAllocateInfo ai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = descriptors; ai.descriptorSetCount = 1; ai.pSetLayouts = &set_layout;
        VkDescriptorSet set;
        result = AllocateDescriptorSets(device, &ai, &set);
        if (result) break;
        VkDescriptorBufferInfo bi = {buffer, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; write.pBufferInfo = &bi;
        UpdateDescriptorSets(device, 1, &write, 0, nullptr);
        VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        result = CreateCommandPool(device, &ci, nullptr, &commands);
        if (result) break;
        VkCommandBufferAllocateInfo ac = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ac.commandPool = commands; ac.commandBufferCount = 1;
        VkCommandBuffer command;
        result = AllocateCommandBuffers(device, &ac, &command);
        if (result) break;
        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        result = BeginCommandBuffer(command, &begin);
        if (result) break;
        VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        CmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        CmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        CmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, nullptr);
        CmdDispatch(command, 16, 1, 1);
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        CmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        result = EndCommandBuffer(command);
        if (result) break;
        VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = CreateFence(device, &fi, nullptr, &fence);
        if (result) break;
        VkQueue q; GetDeviceQueue(device, 0, 0, &q);
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        result = QueueSubmit(q, 1, &submit, fence);
        std::printf("vkQueueSubmit(compute): %d\n", result);
        if (result) break;
        result = WaitForFences(device, 1, &fence, VK_TRUE, 10000000000ULL);
        if (result) break;
        void *mapped = nullptr;
        result = MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
        if (result) break;
        auto words = (volatile uint32_t *)mapped;
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t seed = 0x1234abcdu ^ (i * 7919u);
            uint16_t a = uint16_t(seed ^ (i * 137u));
            uint16_t b = uint16_t(uint32_t(a) * 13u + 17001u);
            int8_t c = int8_t(seed ^ i);
            int8_t d = int8_t(int(c) * 7 + 31);
            uint64_t x = (uint64_t(seed) << 32) | uint64_t(i * 101u + 7u);
            uint64_t y = (x * 0x100000003ULL) ^ (x >> 17);
            float f = float(int(i % 127u) - 63) * 0.5f;
            float g = f * 1.5f + 0.25f;
            uint32_t expected[8] = {b, uint32_t(int16_t(a) >> 3), uint32_t(y), uint32_t(y >> 32), uint32_t(int(d)), uint32_t(int(c) >> 2)};
            memcpy(&expected[6], &g, 4); memcpy(&expected[7], &f, 4);
            for (unsigned k = 0; k < 8; k++) {
                if (words[i * 8 + k] != expected[k]) {
                    std::printf("typed mismatch invocation=%u lane=%u got=0x%x expected=0x%x\n", i, k, words[i * 8 + k], expected[k]);
                    result = VK_ERROR_UNKNOWN;
                    break;
                }
            }
            if (result != VK_SUCCESS) break;
        }
        UnmapMemory(device, memory);
        std::printf("GPU fp16/int16/int64/int8 8192 checked outputs: %s\n", result == VK_SUCCESS ? "PASS" : "FAIL");
    } while (false);
    if (fence) DestroyFence(device, fence, nullptr);
    if (commands) DestroyCommandPool(device, commands, nullptr);
    if (descriptors) DestroyDescriptorPool(device, descriptors, nullptr);
    if (pipeline) DestroyPipeline(device, pipeline, nullptr);
    if (shader) DestroyShaderModule(device, shader, nullptr);
    if (layout) DestroyPipelineLayout(device, layout, nullptr);
    if (set_layout) DestroyDescriptorSetLayout(device, set_layout, nullptr);
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
    const char *extensions[] = {VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME};
    VkPhysicalDeviceFeatures features = {};
    features.shaderInt16 = VK_TRUE; features.shaderInt64 = VK_TRUE;
    VkPhysicalDeviceShaderFloat16Int8Features types = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES};
    types.shaderFloat16 = VK_TRUE; types.shaderInt8 = VK_TRUE;
    info.pEnabledFeatures = &features;
    info.pNext = &types;
    info.enabledExtensionCount = 1; info.ppEnabledExtensionNames = extensions;
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
    bi.size = 32768;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    result = CreateBuffer(device, &bi, nullptr, &buffer);
    if (result == VK_SUCCESS) {
        VkMemoryRequirements requirements = {};
        GetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        const char *type = std::getenv("TYPED_MEMORY_INDEX");
        allocation.memoryTypeIndex = type ? std::strtoul(type, nullptr, 10) : 0;
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

    if (result == VK_SUCCESS) {
        LOAD(CreateCommandPool); LOAD(DestroyCommandPool); LOAD(AllocateCommandBuffers);
        LOAD(BeginCommandBuffer); LOAD(EndCommandBuffer); LOAD(CmdFillBuffer); LOAD(CmdPipelineBarrier);
        LOAD(GetDeviceQueue); LOAD(QueueSubmit); LOAD(CreateFence); LOAD(DestroyFence); LOAD(WaitForFences);
        VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        VkCommandPool pool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        result = CreateCommandPool(device, &pool_info, nullptr, &pool);
        VkCommandBuffer command = VK_NULL_HANDLE;
        if (result == VK_SUCCESS) {
            VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            alloc.commandPool = pool;
            alloc.commandBufferCount = 1;
            result = AllocateCommandBuffers(device, &alloc, &command);
        }
        if (result == VK_SUCCESS) {
            VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            result = BeginCommandBuffer(command, &begin);
            if (result == VK_SUCCESS) {
                CmdFillBuffer(command, buffer, 0, VK_WHOLE_SIZE, 0x1234abcd);
                VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                CmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
                result = EndCommandBuffer(command);
            }
        }
        if (result == VK_SUCCESS) {
            VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            result = CreateFence(device, &fi, nullptr, &fence);
        }
        if (result == VK_SUCCESS) {
            VkQueue q;
            GetDeviceQueue(device, 0, 0, &q);
            VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            result = QueueSubmit(q, 1, &submit, fence);
            std::printf("vkQueueSubmit(fill): %d\n", result);
            if (result == VK_SUCCESS) result = WaitForFences(device, 1, &fence, VK_TRUE, 10000000000ULL);
            std::printf("vkWaitForFences: %d\n", result);
        }
        if (result == VK_SUCCESS) {
            void *mapping = nullptr;
            result = MapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapping);
            if (result == VK_SUCCESS) {
                auto words = (volatile uint32_t *)mapping;
                MemoryBarrier();
                for (uint32_t i = 0; i < 1024; i++) {
                    if (words[i] != 0x1234abcd) {
                        std::printf("GPU fill mismatch [%u]=0x%x\n", i, words[i]);
                        result = VK_ERROR_UNKNOWN;
                        break;
                    }
                }
                UnmapMemory(device, memory);
            }
            std::printf("GPU fill readback: %d\n", result);
        }
        if (fence) DestroyFence(device, fence, nullptr);
        if (pool) DestroyCommandPool(device, pool, nullptr);
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
    const char *instance_extensions[] = {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    info.enabledExtensionCount = 1; info.ppEnabledExtensionNames = instance_extensions;
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
