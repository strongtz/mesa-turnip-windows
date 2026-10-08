#include <windows.h>
#undef CreateSemaphore
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

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

int main()
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
    const char *extension="VK_KHR_timeline_semaphore";
    VkDeviceCreateInfo di={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.pNext=&tf; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi;
    di.enabledExtensionCount=1; di.ppEnabledExtensionNames=&extension;
    VkDevice device; CHECK(cd(physical,&di,nullptr,&device));
#define LOAD(n) n=(PFN_vk##n)gdpa(device,"vk" #n); REQUIRE(n);
    FUNCTIONS(LOAD)
    SignalSemaphore=(PFN_vkSignalSemaphoreKHR)gdpa(device,"vkSignalSemaphoreKHR"); REQUIRE(SignalSemaphore);
    WaitSemaphores=(PFN_vkWaitSemaphoresKHR)gdpa(device,"vkWaitSemaphoresKHR"); REQUIRE(WaitSemaphores);
    GetSemaphoreCounterValue=(PFN_vkGetSemaphoreCounterValueKHR)gdpa(device,"vkGetSemaphoreCounterValueKHR"); REQUIRE(GetSemaphoreCounterValue);
    VkQueue queues[2]; GetDeviceQueue(device,0,0,&queues[0]); GetDeviceQueue(device,0,1,&queues[1]);
    VkSemaphoreTypeCreateInfo type={VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO}; type.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE; type.initialValue=3;
    VkSemaphoreCreateInfo si={VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; si.pNext=&type;
    VkSemaphore timeline; CHECK(CreateSemaphore(device,&si,nullptr,&timeline));
    uint64_t value=0; CHECK(GetSemaphoreCounterValue(device,timeline,&value)); REQUIRE(value==3);
    VkSemaphoreWaitInfo wi={VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO}; wi.semaphoreCount=1; wi.pSemaphores=&timeline; wi.pValues=&value;
    value=4; REQUIRE(WaitSemaphores(device,&wi,0)==VK_TIMEOUT);
    VkFenceCreateInfo fi={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fence;
    CHECK(CreateFence(device,&fi,nullptr,&fence)); REQUIRE(GetFenceStatus(device,fence)==VK_NOT_READY);
    VkPipelineStageFlags stage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    uint64_t wait_value=4,signal_value=5;
    VkTimelineSemaphoreSubmitInfo ts={VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    ts.waitSemaphoreValueCount=1; ts.pWaitSemaphoreValues=&wait_value;
    ts.signalSemaphoreValueCount=1; ts.pSignalSemaphoreValues=&signal_value;
    VkSubmitInfo submit={VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.pNext=&ts;
    submit.waitSemaphoreCount=1; submit.pWaitSemaphores=&timeline; submit.pWaitDstStageMask=&stage;
    submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&timeline;
    std::thread release([&] {
        Sleep(200);
        VkSemaphoreSignalInfo signal={VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO}; signal.semaphore=timeline; signal.value=4;
        CHECK(SignalSemaphore(device,&signal));
    });
    auto start=std::chrono::steady_clock::now(); CHECK(QueueSubmit(queues[0],1,&submit,fence));
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    printf("wait-before-host-signal submit %.3f ms\n",ms); REQUIRE(ms<150);
    REQUIRE(WaitForFences(device,1,&fence,VK_TRUE,0)==VK_TIMEOUT);
    CHECK(WaitForFences(device,1,&fence,VK_TRUE,5000000000ULL)); release.join();
    CHECK(GetSemaphoreCounterValue(device,timeline,&value)); REQUIRE(value==5);
    puts("host signal, GPU wait-before-signal, asynchronous submit, counter, timeout: PASS");

    VkBufferCreateInfo bi={VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=65536; bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer; CHECK(CreateBuffer(device,&bi,nullptr,&buffer));
    VkMemoryRequirements req; GetBufferMemoryRequirements(device,buffer,&req);
    VkMemoryAllocateInfo ai={VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize=req.size; ai.memoryTypeIndex=1;
    VkDeviceMemory memory; CHECK(AllocateMemory(device,&ai,nullptr,&memory)); CHECK(BindBufferMemory(device,buffer,memory,0));
    VkCommandPoolCreateInfo pi={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; VkCommandPool pool; CHECK(CreateCommandPool(device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ca={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool=pool; ca.commandBufferCount=1;
    VkCommandBuffer command; CHECK(AllocateCommandBuffers(device,&ca,&command));
    VkCommandBufferBeginInfo begin={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(BeginCommandBuffer(command,&begin));
    CmdFillBuffer(command,buffer,0,VK_WHOLE_SIZE,0xa1b2c3d4);
    VkMemoryBarrier mb={VK_STRUCTURE_TYPE_MEMORY_BARRIER}; mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);
    CHECK(EndCommandBuffer(command));
    CHECK(ResetFences(device,1,&fence));
    wait_value=7; signal_value=8; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
    CHECK(QueueSubmit(queues[0],1,&submit,fence));
    REQUIRE(GetFenceStatus(device,fence)==VK_NOT_READY);
    uint64_t seven=7; VkTimelineSemaphoreSubmitInfo ts2={VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    ts2.signalSemaphoreValueCount=1; ts2.pSignalSemaphoreValues=&seven;
    VkSubmitInfo producer={VK_STRUCTURE_TYPE_SUBMIT_INFO}; producer.pNext=&ts2; producer.signalSemaphoreCount=1; producer.pSignalSemaphores=&timeline;
    CHECK(QueueSubmit(queues[1],1,&producer,VK_NULL_HANDLE));
    CHECK(WaitForFences(device,1,&fence,VK_TRUE,5000000000ULL));
    void *map; CHECK(MapMemory(device,memory,0,VK_WHOLE_SIZE,0,&map));
    MemoryBarrier(); for(unsigned i=0;i<16384;i++) REQUIRE(((uint32_t*)map)[i]==0xa1b2c3d4);
    UnmapMemory(device,memory);
    puts("cross-queue wait submitted before signal + GPU fill/readback: PASS");

    si.pNext=nullptr; VkSemaphore binary; CHECK(CreateSemaphore(device,&si,nullptr,&binary));
    for(unsigned i=0;i<64;i++) {
        CHECK(ResetFences(device,1,&fence));
        VkSubmitInfo signal={VK_STRUCTURE_TYPE_SUBMIT_INFO}; signal.signalSemaphoreCount=1; signal.pSignalSemaphores=&binary;
        CHECK(QueueSubmit(queues[0],1,&signal,VK_NULL_HANDLE));
        VkSubmitInfo wait={VK_STRUCTURE_TYPE_SUBMIT_INFO}; wait.waitSemaphoreCount=1; wait.pWaitSemaphores=&binary; wait.pWaitDstStageMask=&stage;
        CHECK(QueueSubmit(queues[1],1,&wait,fence)); CHECK(WaitForFences(device,1,&fence,VK_TRUE,5000000000ULL));
    }
    puts("64 binary signal/wait reuse + fence reset cycles: PASS");
    VkSemaphore returned; CHECK(CreateSemaphore(device,&si,nullptr,&returned));
    CHECK(ResetFences(device,1,&fence));
    uint64_t gate_value=9;
    VkTimelineSemaphoreSubmitInfo gate_ts={VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    gate_ts.waitSemaphoreValueCount=1; gate_ts.pWaitSemaphoreValues=&gate_value;
    VkSubmitInfo gate={VK_STRUCTURE_TYPE_SUBMIT_INFO}; gate.pNext=&gate_ts;
    gate.waitSemaphoreCount=1; gate.pWaitSemaphores=&timeline; gate.pWaitDstStageMask=&stage;
    CHECK(QueueSubmit(queues[0],1,&gate,VK_NULL_HANDLE));
    for(unsigned i=0;i<128;i++) {
        VkSubmitInfo a={VK_STRUCTURE_TYPE_SUBMIT_INFO};
        a.waitSemaphoreCount=i?1:0; a.pWaitSemaphores=&returned; a.pWaitDstStageMask=&stage;
        a.signalSemaphoreCount=1; a.pSignalSemaphores=&binary;
        CHECK(QueueSubmit(queues[0],1,&a,VK_NULL_HANDLE));
        VkSubmitInfo b={VK_STRUCTURE_TYPE_SUBMIT_INFO};
        b.waitSemaphoreCount=1; b.pWaitSemaphores=&binary; b.pWaitDstStageMask=&stage;
        b.signalSemaphoreCount=1; b.pSignalSemaphores=&returned;
        CHECK(QueueSubmit(queues[1],1,&b,i==127?fence:VK_NULL_HANDLE));
    }
    REQUIRE(GetFenceStatus(device,fence)==VK_NOT_READY);
    VkSemaphoreSignalInfo release_gate={VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO}; release_gate.semaphore=timeline; release_gate.value=9;
    CHECK(SignalSemaphore(device,&release_gate));
    CHECK(WaitForFences(device,1,&fence,VK_TRUE,5000000000ULL));
    puts("256 pending cross-queue submissions, binary payload generations, host gate: PASS");
    DestroySemaphore(device,returned,nullptr);
    si.pNext=&type; VkSemaphore other; CHECK(CreateSemaphore(device,&si,nullptr,&other));
    VkSemaphore pair[2]={timeline,other}; uint64_t targets[2]={1ULL<<40,3};
    VkSemaphoreWaitInfo any={VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO}; any.semaphoreCount=2; any.pSemaphores=pair; any.pValues=targets;
    any.flags=VK_SEMAPHORE_WAIT_ANY_BIT; CHECK(WaitSemaphores(device,&any,0));
    any.flags=0; REQUIRE(WaitSemaphores(device,&any,0)==VK_TIMEOUT);
    release_gate.value=1ULL<<40; CHECK(SignalSemaphore(device,&release_gate)); CHECK(WaitSemaphores(device,&any,1000000000ULL));
    CHECK(GetSemaphoreCounterValue(device,timeline,&value)); REQUIRE(value==(1ULL<<40));
    DestroySemaphore(device,other,nullptr);
    puts("timeline wait-any/wait-all and 64-bit host values: PASS");
    type.initialValue=UINT64_MAX;
    CHECK(CreateSemaphore(device,&si,nullptr,&other));
    CHECK(GetSemaphoreCounterValue(device,other,&value)); REQUIRE(value==UINT64_MAX);
    VkSemaphoreWaitInfo max_wait={VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    uint64_t max_value=UINT64_MAX;
    max_wait.semaphoreCount=1; max_wait.pSemaphores=&other; max_wait.pValues=&max_value;
    CHECK(WaitSemaphores(device,&max_wait,0));
    DestroySemaphore(device,other,nullptr);
    release_gate.value=UINT64_MAX; CHECK(SignalSemaphore(device,&release_gate));
    CHECK(GetSemaphoreCounterValue(device,timeline,&value)); REQUIRE(value==UINT64_MAX);
    max_wait.pSemaphores=&timeline; CHECK(WaitSemaphores(device,&max_wait,0));
    puts("UINT64_MAX initial value, host signal, counter and wait: PASS");
    CHECK(QueueWaitIdle(queues[0])); CHECK(QueueWaitIdle(queues[1])); CHECK(DeviceWaitIdle(device));
    DestroySemaphore(device,binary,nullptr); DestroySemaphore(device,timeline,nullptr); DestroyFence(device,fence,nullptr);
    DestroyCommandPool(device,pool,nullptr); DestroyBuffer(device,buffer,nullptr); FreeMemory(device,memory,nullptr);
    DestroyDevice(device,nullptr);
    ((PFN_vkDestroyInstance)get(instance,"vkDestroyInstance"))(instance,nullptr);
    puts("synchronization test: PASS");
}
