#include <windows.h>
#include <cstdio>

int main(int argc, char **argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2)
        return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    using GetProc = void *(*)(void *, const char *);
    using Negotiate = int (*)(unsigned *);
    auto get = reinterpret_cast<GetProc>(GetProcAddress(module, "vk_icdGetInstanceProcAddr"));
    auto negotiate = reinterpret_cast<Negotiate>(GetProcAddress(module, "vk_icdNegotiateLoaderICDInterfaceVersion"));
    unsigned version = 7;
    bool ok = get && negotiate && negotiate(&version) == 0 &&
              get(nullptr, "vkCreateInstance") &&
              get(nullptr, "vkEnumerateInstanceExtensionProperties") &&
              !get(nullptr, "vkThisEntrypointDoesNotExist");
    std::printf("ICD load, negotiation and entrypoint lookup: %s\n", ok ? "PASS" : "FAIL");
    FreeLibrary(module);
    return ok ? 0 : 1;
}
