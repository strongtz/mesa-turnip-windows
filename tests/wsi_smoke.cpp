#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <dwmapi.h>
#undef CreateSemaphore
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")

#define REQUIRE(e) do { if (!(e)) { printf("FAIL %s line %d\n", #e, __LINE__); std::exit(1); } } while (0)
#define CHECK(e) do { VkResult r = (e); if (r != VK_SUCCESS) { printf("FAIL %s = %d line %d\n", #e, r, __LINE__); std::exit(1); } } while (0)
#define DEVICE_FUNCTIONS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(CreateSwapchainKHR) X(DestroySwapchainKHR) \
 X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) X(QueuePresentKHR) X(WaitForPresentKHR) \
 X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandBuffer) \
 X(BeginCommandBuffer) X(EndCommandBuffer) X(CmdPipelineBarrier) X(CmdClearColorImage) \
 X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) \
 X(CreateSemaphore) X(DestroySemaphore) X(QueueSubmit) X(DeviceWaitIdle)
#define DECLARE(n) static PFN_vk##n n;
DEVICE_FUNCTIONS(DECLARE)

static void pump() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SetProcessDPIAware();
    auto module = LoadLibraryW(L"vulkan-1.dll"); REQUIRE(module);
    auto get = (PFN_vkGetInstanceProcAddr)GetProcAddress(module, "vkGetInstanceProcAddr"); REQUIRE(get);
    auto create = (PFN_vkCreateInstance)get(nullptr, "vkCreateInstance");
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_3;
    const char *iext[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ii.pApplicationInfo = &app;
    ii.enabledExtensionCount = 2; ii.ppEnabledExtensionNames = iext;
    VkInstance instance; CHECK(create(&ii, nullptr, &instance));
#define INSTANCE(n) auto n = (PFN_vk##n)get(instance, "vk" #n); REQUIRE(n)
    INSTANCE(EnumeratePhysicalDevices); INSTANCE(CreateDevice); INSTANCE(GetDeviceProcAddr);
    INSTANCE(CreateWin32SurfaceKHR); INSTANCE(DestroySurfaceKHR); INSTANCE(DestroyInstance);
    INSTANCE(GetPhysicalDeviceSurfaceCapabilitiesKHR); INSTANCE(GetPhysicalDeviceSurfaceFormatsKHR);
    INSTANCE(GetPhysicalDeviceSurfacePresentModesKHR);
    uint32_t count = 1; VkPhysicalDevice physical;
    CHECK(EnumeratePhysicalDevices(instance, &count, &physical));
    float priority = 1;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueCount = 1; qi.pQueuePriorities = &priority;
    VkPhysicalDevicePresentWaitFeaturesKHR wait = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR}; wait.presentWait = VK_TRUE;
    VkPhysicalDevicePresentIdFeaturesKHR id = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR}; id.presentId = VK_TRUE; id.pNext = &wait;
    const char *dext[] = {"VK_KHR_swapchain", "VK_KHR_present_id", "VK_KHR_present_wait"};
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.pNext = &id;
    di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi; di.enabledExtensionCount = 3; di.ppEnabledExtensionNames = dext;
    VkDevice device; CHECK(CreateDevice(physical, &di, nullptr, &device));
#define LOAD(n) n = (PFN_vk##n)GetDeviceProcAddr(device, "vk" #n); REQUIRE(n);
    DEVICE_FUNCTIONS(LOAD)
    VkQueue queue; GetDeviceQueue(device, 0, 0, &queue);
    WNDCLASSW wc = {}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"TurnipNativeWsiTest";
    REQUIRE(RegisterClassW(&wc));
    HWND wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName,
        L"Turnip native DXGI pixel test", WS_OVERLAPPEDWINDOW, 80, 80, 320, 260,
        nullptr, nullptr, wc.hInstance, nullptr); REQUIRE(wnd);
    ShowWindow(wnd, SW_SHOWNOACTIVATE); pump();
    VkWin32SurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR}; sci.hinstance = wc.hInstance; sci.hwnd = wnd;
    VkSurfaceKHR surface; CHECK(CreateWin32SurfaceKHR(instance, &sci, nullptr, &surface));
    CHECK(GetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(count); CHECK(GetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data()));
    CHECK(GetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr));
    std::vector<VkPresentModeKHR> modes(count); CHECK(GetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.data()));
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool; CHECK(CreateCommandPool(device, &poolInfo, nullptr, &pool));
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool = pool; ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount = 1;
    VkCommandBuffer cb; CHECK(AllocateCommandBuffers(device, &ca, &cb));
    VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence acquired, submitted;
    CHECK(CreateFence(device, &fi, nullptr, &acquired)); CHECK(CreateFence(device, &fi, nullptr, &submitted));
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkSemaphore rendered;
    CHECK(CreateSemaphore(device, &semInfo, nullptr, &rendered));
    unsigned checks = 0, chains = 0;
    for (auto format : formats) for (auto mode : modes) {
        if (mode != VK_PRESENT_MODE_FIFO_KHR && mode != VK_PRESENT_MODE_IMMEDIATE_KHR && mode != VK_PRESENT_MODE_MAILBOX_KHR) continue;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        for (int resize = 0; resize < 2; ++resize) {
            RECT wr = {0, 0, resize ? 384 : 257, resize ? 241 : 193};
            AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
            REQUIRE(SetWindowPos(wnd, HWND_TOPMOST, 80, 80, wr.right - wr.left, wr.bottom - wr.top, SWP_NOACTIVATE)); pump();
            VkSurfaceCapabilitiesKHR caps; CHECK(GetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps));
            VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; ci.surface = surface;
            ci.minImageCount = caps.minImageCount; ci.imageFormat = format.format; ci.imageColorSpace = format.colorSpace;
            ci.imageExtent = caps.currentExtent; ci.imageArrayLayers = 1; ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            ci.preTransform = caps.currentTransform; ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = mode;
            ci.clipped = VK_TRUE; ci.oldSwapchain = swapchain;
            VkSwapchainKHR next; CHECK(CreateSwapchainKHR(device, &ci, nullptr, &next));
            if (swapchain) DestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = next; ++chains;
            CHECK(GetSwapchainImagesKHR(device, swapchain, &count, nullptr));
            std::vector<VkImage> images(count); CHECK(GetSwapchainImagesKHR(device, swapchain, &count, images.data()));
            for (uint64_t frame = 0; frame < 4; ++frame) {
                uint32_t index; CHECK(AcquireNextImageKHR(device, swapchain, 5000000000ULL, VK_NULL_HANDLE, acquired, &index));
                CHECK(WaitForFences(device, 1, &acquired, VK_TRUE, 5000000000ULL)); CHECK(ResetFences(device, 1, &acquired));
                CHECK(ResetCommandBuffer(cb, 0));
                VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(BeginCommandBuffer(cb, &begin));
                VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = images[index]; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                VkClearColorValue color = {}; color.float32[3] = 1;
                if (frame < 3) color.float32[frame] = 1; else { color.float32[0] = 0.25f; color.float32[1] = 0.5f; color.float32[2] = 0.75f; }
                CmdClearColorImage(cb, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = 0;
                CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                CHECK(EndCommandBuffer(cb));
                VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &cb;
                submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &rendered; CHECK(QueueSubmit(queue, 1, &submit, submitted));
                uint64_t presentId = frame + 1;
                VkPresentIdKHR presentIds = {VK_STRUCTURE_TYPE_PRESENT_ID_KHR}; presentIds.swapchainCount = 1; presentIds.pPresentIds = &presentId;
                VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; present.pNext = &presentIds;
                present.waitSemaphoreCount = 1; present.pWaitSemaphores = &rendered; present.swapchainCount = 1; present.pSwapchains = &swapchain; present.pImageIndices = &index;
                CHECK(QueuePresentKHR(queue, &present));
                CHECK(WaitForPresentKHR(device, swapchain, presentId, 5000000000ULL));
                CHECK(WaitForFences(device, 1, &submitted, VK_TRUE, 5000000000ULL)); CHECK(ResetFences(device, 1, &submitted));
                int expected[3];
                for (int c = 0; c < 3; ++c) {
                    float v = color.float32[c];
                    if (format.format == VK_FORMAT_B8G8R8A8_SRGB) v = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
                    expected[c] = (int)std::lround(v * 255);
                }
                bool matched = false; COLORREF pixel = 0;
                for (int retry = 0; retry < 50 && !matched; ++retry) {
                    pump(); DwmFlush(); Sleep(10);
                    HDC screen = GetDC(nullptr); matched = true;
                    POINT points[] = {{8,8}, {(LONG)ci.imageExtent.width - 9,8}, {8,(LONG)ci.imageExtent.height - 9}, {(LONG)ci.imageExtent.width - 9,(LONG)ci.imageExtent.height - 9}, {(LONG)ci.imageExtent.width / 2,(LONG)ci.imageExtent.height / 2}};
                    for (auto pt : points) {
                        ClientToScreen(wnd, &pt); pixel = GetPixel(screen, pt.x, pt.y);
                        matched &= pixel != CLR_INVALID && abs((int)GetRValue(pixel) - expected[0]) <= 4 && abs((int)GetGValue(pixel) - expected[1]) <= 4 && abs((int)GetBValue(pixel) - expected[2]) <= 4;
                    }
                    ReleaseDC(nullptr, screen);
                }
                printf("format=%d mode=%d extent=%ux%u frame=%llu screen=%u,%u,%u expected=%d,%d,%d %s\n", format.format, mode, ci.imageExtent.width, ci.imageExtent.height, frame, GetRValue(pixel), GetGValue(pixel), GetBValue(pixel), expected[0], expected[1], expected[2], matched ? "PASS" : "FAIL");
                REQUIRE(matched); checks += 5;
            }
            CHECK(DeviceWaitIdle(device));
        }
        DestroySwapchainKHR(device, swapchain, nullptr);
    }
    DestroySemaphore(device, rendered, nullptr); DestroyFence(device, acquired, nullptr); DestroyFence(device, submitted, nullptr);
    DestroyCommandPool(device, pool, nullptr); DestroySurfaceKHR(instance, surface, nullptr);
    DestroyWindow(wnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
    DestroyDevice(device, nullptr); DestroyInstance(instance, nullptr); FreeLibrary(module);
    printf("PASS: %u swapchains, %u displayed pixel checks, resize/recreate/cleanup\n", chains, checks);
    return 0;
}
