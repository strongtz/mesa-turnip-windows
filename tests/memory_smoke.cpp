#include <windows.h>
#undef CreateSemaphore
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

#define FUNCTIONS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(CreateSemaphore) X(DestroySemaphore) \
 X(CreateFence) X(DestroyFence) X(ResetFences) X(GetFenceStatus) X(WaitForFences) \
 X(QueueSubmit) X(QueueWaitIdle) X(DeviceWaitIdle) X(CreateBuffer) X(DestroyBuffer) \
 X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) \
 X(MapMemory) X(UnmapMemory) X(CreateCommandPool) X(DestroyCommandPool) \
 X(AllocateCommandBuffers) X(BeginCommandBuffer) X(EndCommandBuffer) \
 X(CmdFillBuffer) X(CmdPipelineBarrier)
#define DECLARE(n) static PFN_vk##n n;
FUNCTIONS(DECLARE)
static PFN_vkSignalSemaphoreKHR SignalSemaphore;
static PFN_vkWaitSemaphoresKHR WaitSemaphores;
static PFN_vkGetSemaphoreCounterValueKHR GetSemaphoreCounterValue;
#define CHECK(e) do { VkResult r=(e); if(r!=VK_SUCCESS) { printf("FAIL %s = %d line %d\n",#e,r,__LINE__); std::exit(1); } } while(0)
#define REQUIRE(e) do { if(!(e)) { printf("FAIL %s line %d\n",#e,__LINE__); std::exit(1); } } while(0)

int main(int argc, char **argv)
{
    setvbuf(stdout,nullptr,_IONBF,0);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto module=LoadLibraryA("vulkan-1.dll"); REQUIRE(module);
    auto get=(PFN_vkGetInstanceProcAddr)GetProcAddress(module,"vkGetInstanceProcAddr"); REQUIRE(get);
    auto create=(PFN_vkCreateInstance)get(nullptr,"vkCreateInstance");
    VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_1;
    VkInstanceCreateInfo ii={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ii.pApplicationInfo=&app;
    VkInstance instance; CHECK(create(&ii,nullptr,&instance));
    auto enumerate=(PFN_vkEnumeratePhysicalDevices)get(instance,"vkEnumeratePhysicalDevices");
    uint32_t count=1; VkPhysicalDevice physical; CHECK(enumerate(instance,&count,&physical));
    auto cd=(PFN_vkCreateDevice)get(instance,"vkCreateDevice");
    auto gdpa=(PFN_vkGetDeviceProcAddr)get(instance,"vkGetDeviceProcAddr");
    float priorities[2]={1,1};
    VkDeviceQueueCreateInfo qi={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueCount=2; qi.pQueuePriorities=priorities;
    VkPhysicalDeviceTimelineSemaphoreFeatures tf={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES}; tf.timelineSemaphore=VK_TRUE;
    const char *extensions[]={"VK_KHR_timeline_semaphore","VK_KHR_buffer_device_address","VK_EXT_memory_budget"};
    VkPhysicalDeviceBufferDeviceAddressFeatures bda={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
    bda.bufferDeviceAddress=VK_TRUE; tf.pNext=&bda;
    VkDeviceCreateInfo di={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.pNext=&tf; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi;
    di.enabledExtensionCount=3; di.ppEnabledExtensionNames=extensions;
    VkDevice device; CHECK(cd(physical,&di,nullptr,&device));
#define LOAD(n) n=(PFN_vk##n)gdpa(device,"vk" #n); REQUIRE(n);
    FUNCTIONS(LOAD)
    SignalSemaphore=(PFN_vkSignalSemaphoreKHR)gdpa(device,"vkSignalSemaphoreKHR"); REQUIRE(SignalSemaphore);
    WaitSemaphores=(PFN_vkWaitSemaphoresKHR)gdpa(device,"vkWaitSemaphoresKHR"); REQUIRE(WaitSemaphores);
    GetSemaphoreCounterValue=(PFN_vkGetSemaphoreCounterValueKHR)gdpa(device,"vkGetSemaphoreCounterValueKHR"); REQUIRE(GetSemaphoreCounterValue);
    VkQueue queues[2]; GetDeviceQueue(device,0,0,&queues[0]); GetDeviceQueue(device,0,1,&queues[1]);

    auto get_mem=(PFN_vkGetPhysicalDeviceMemoryProperties2)get(instance,"vkGetPhysicalDeviceMemoryProperties2");
    auto address=(PFN_vkGetBufferDeviceAddressKHR)gdpa(device,"vkGetBufferDeviceAddressKHR"); REQUIRE(address);
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 mp={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2}; mp.pNext=&budget;
    auto report=[&](const char *label) {
        get_mem(physical,&mp);
        printf("%s usage=%llu budget=%llu heap=%llu\n",label,budget.heapUsage[0],budget.heapBudget[0],mp.memoryProperties.memoryHeaps[0].size);
        REQUIRE(budget.heapBudget[0]>0 && budget.heapBudget[0]<=mp.memoryProperties.memoryHeaps[0].size);
    };
    report("before"); uint64_t before=budget.heapUsage[0];
    if (argc > 1 && !std::strcmp(argv[1], "--pressure")) {
        std::vector<VkDeviceMemory> held;
        constexpr VkDeviceSize chunk=512ULL*1024*1024;
        constexpr uint64_t reserve=4ULL*1024*1024*1024;
        const uint64_t limit=budget.heapBudget[0]+2*chunk;
        bool exhausted=false;
        while (held.size()*chunk < limit) {
            MEMORYSTATUSEX system={sizeof(system)};
            REQUIRE(GlobalMemoryStatusEx(&system));
            if (system.ullAvailPhys < reserve+chunk) {
                puts("Pressure test stopped at system free-memory reserve");
                break;
            }
            VkMemoryAllocateInfo pressure={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            pressure.allocationSize=chunk; pressure.memoryTypeIndex=1;
            VkDeviceMemory allocation=VK_NULL_HANDLE;
            VkResult allocation_result=AllocateMemory(device,&pressure,nullptr,&allocation);
            printf("pressure allocation %zu: %d\n",held.size(),allocation_result);
            if (allocation_result==VK_ERROR_OUT_OF_DEVICE_MEMORY) { exhausted=true; break; }
            CHECK(allocation_result); held.push_back(allocation);
        }
        report("pressure peak");
        for (auto allocation:held) FreeMemory(device,allocation,nullptr);
        report("pressure released");
        REQUIRE(budget.heapUsage[0]<before+chunk);
        if (!exhausted) {
            DestroyDevice(device,nullptr);
            ((PFN_vkDestroyInstance)get(instance,"vkDestroyInstance"))(instance,nullptr);
            puts("Pressure test inconclusive: allocation failure not reached within limits");
            return 2;
        }
        puts("Allocation exhaustion returned OUT_OF_DEVICE_MEMORY and released held memory: PASS");
    }
    constexpr unsigned N=6; constexpr VkDeviceSize size=900ULL*1024*1024;
    VkBuffer buffers[N]={}; VkDeviceMemory memories[N]={}; VkDeviceAddress addresses[N]={};
    VkCommandPoolCreateInfo pi={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; VkCommandPool pool; CHECK(CreateCommandPool(device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ca={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool=pool; ca.commandBufferCount=1;
    VkCommandBuffer command; CHECK(AllocateCommandBuffers(device,&ca,&command));
    VkCommandBufferBeginInfo begin={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(BeginCommandBuffer(command,&begin));
    for(unsigned i=0;i<N;i++) {
        VkBufferCreateInfo bi={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=size;
        bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        CHECK(CreateBuffer(device,&bi,nullptr,&buffers[i]));
        VkMemoryRequirements req; GetBufferMemoryRequirements(device,buffers[i],&req);
        VkMemoryAllocateFlagsInfo flags={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.pNext=&flags; ai.allocationSize=req.size; ai.memoryTypeIndex=1;
        CHECK(AllocateMemory(device,&ai,nullptr,&memories[i])); CHECK(BindBufferMemory(device,buffers[i],memories[i],0));
        VkBufferDeviceAddressInfo info={VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; info.buffer=buffers[i]; addresses[i]=address(device,&info);
        printf("buffer[%u] address=0x%llx size=%llu\n",i,addresses[i],size);
        if (GetEnvironmentVariableA("TU_WDDM_HIGH_VA",nullptr,0)) REQUIRE(addresses[i]>0xffffffffULL);
        for(unsigned j=0;j<i;j++) REQUIRE(addresses[i]+size<=addresses[j] || addresses[j]+size<=addresses[i]);
        CmdFillBuffer(command,buffers[i],0,65536,0x12340000+i);
        CmdFillBuffer(command,buffers[i],size-65536,65536,0xabcd0000+i);
    }
    bool high_va=false; for(auto a:addresses) high_va|=a>0xffffffffULL; REQUIRE(high_va);
    report("allocated"); REQUIRE(budget.heapUsage[0]>=before+N*size);
    VkMemoryBarrier mb={VK_STRUCTURE_TYPE_MEMORY_BARRIER}; mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);
    CHECK(EndCommandBuffer(command));
    VkSubmitInfo submit={VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
    CHECK(QueueSubmit(queues[0],1,&submit,VK_NULL_HANDLE)); CHECK(QueueWaitIdle(queues[0]));
    for(unsigned i=0;i<N;i++) {
        void *map; CHECK(MapMemory(device,memories[i],0,VK_WHOLE_SIZE,0,&map)); MemoryBarrier();
        auto first=(uint32_t *)map; auto last=(uint32_t *)((char *)map+size-65536);
        for(unsigned j=0;j<16384;j++) { REQUIRE(first[j]==0x12340000+i); REQUIRE(last[j]==0xabcd0000+i); }
        UnmapMemory(device,memories[i]); DestroyBuffer(device,buffers[i],nullptr); FreeMemory(device,memories[i],nullptr);
    }
    DestroyCommandPool(device,pool,nullptr); CHECK(DeviceWaitIdle(device));
    report("freed"); REQUIRE(budget.heapUsage[0]<before+size);
    DestroyDevice(device,nullptr);
    ((PFN_vkDestroyInstance)get(instance,"vkDestroyInstance"))(instance,nullptr);
    puts("six simultaneous allocations, 5400 MiB total, high-VA GPU writes, budget and cleanup: PASS");
}
