#include "stdafx.h"
#include <d3d9.h>
#include <D3dx9.h>
#pragma comment(lib, "D3dx9.lib")

namespace AffinityChanges
{
    DWORD_PTR gameThreadAffinity = 0;
    static bool Init()
    {
        DWORD_PTR processAffinity, systemAffinity;
        if (!GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity))
        {
            return false;
        }

        DWORD_PTR otherCoresAff = (processAffinity - 1) & processAffinity;
        if (otherCoresAff == 0) // Only one core is available for the game
        {
            return false;
        }
        gameThreadAffinity = processAffinity & ~otherCoresAff;

        SetThreadAffinityMask(GetCurrentThread(), gameThreadAffinity);

        return true;
    }

    static HANDLE WINAPI CreateThread_GameThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress,
        PVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
    {
        HANDLE hThread = CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags | CREATE_SUSPENDED, lpThreadId);
        if (hThread != nullptr)
        {
            SetThreadAffinityMask(hThread, gameThreadAffinity);
            if ((dwCreationFlags & CREATE_SUSPENDED) == 0) // Resume only if the game didn't pass CREATE_SUSPENDED
            {
                ResumeThread(hThread);
            }
        }
        return hThread;
    }

    static void ReplaceFunction(void** funcPtr)
    {
        DWORD dwProtect;

        VirtualProtect(funcPtr, sizeof(*funcPtr), PAGE_READWRITE, &dwProtect);
        *funcPtr = &CreateThread_GameThread;
        VirtualProtect(funcPtr, sizeof(*funcPtr), dwProtect, &dwProtect);
    }

    static bool RedirectImports(HMODULE module_handle)
    {
        const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(module_handle);
        const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);

        // Find IAT
        PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        for (; pImports->Name != 0; pImports++)
        {
            if (_stricmp(reinterpret_cast<const char*>(instance + pImports->Name), "kernel32.dll") == 0)
            {
                if (pImports->OriginalFirstThunk != 0)
                {
                    const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

                    for (ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++)
                    {
                        if (strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "CreateThread") == 0)
                        {
                            void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
                            ReplaceFunction(pAddress);
                            return true;
                        }
                    }
                }
                else
                {
                    void** pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr; j++)
                    {
                        if (pFunctions[j] == &CreateThread)
                        {
                            ReplaceFunction(&pFunctions[j]);
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
}

float gVisibility = 1.0f;
int32_t gBlacklistIndicators = 0;
uint32_t gColor;
float* __cdecl FGetHSV(float* dest, uint8_t H, uint8_t S, uint8_t V)
{
    auto bChangeColor = ((H == 0x41 && S == 0x33) || (H == 0x5B && S == 0 && V == 0xFF) || (H == 0x2B && S == 0x40 && V == 0xFF)); //goggles || green ind || yellow ind
    if (bChangeColor)
    {
        if (gColor)
        {
            switch (gColor)
            {
            case 2:
                dest[0] = 1.0f;
                dest[1] = 0.0f;
                dest[2] = 0.0f;
                dest[3] = 1.0f;
                break;
            case 3:
                dest[0] = 1.0f;
                dest[1] = 0.5f;
                dest[2] = 0.0f;
                dest[3] = 1.0f;
                break;
            default:
                dest[0] = 0.0f;
                dest[1] = 1.0f;
                dest[2] = 0.0f;
                dest[3] = 1.0f;
                break;
            }
            if (gBlacklistIndicators)
            {
                dest[0] *= gVisibility;
                dest[1] *= gVisibility;
                dest[2] *= gVisibility;
                dest[3] *= gVisibility;
            }
            return dest;
        }
    }

    float r, g, b, a = 1.0f;
    float v14 = (float)H * 6.0f * 0.00390625f;
    float v4 = floor(v14);
    float v5 = (float)(255 - S) * 0.0039215689f;
    float v6 = (float)V * 0.0039215689f;
    float v16 = (1.0f - v5) * v6;
    float v10 = (1.0f - (v5 * (v14 - v4))) * v6;
    float v7 = (1.0f - (v14 - v4)) * v5;
    float v15 = (float)V * 0.0039215689f;
    float v17 = (1.0f - v7) * v6;

    switch ((uint32_t)v4)
    {
    case 0:
        r = v15;
        g = v17;
        b = v16;
        break;
    case 1:
        r = v10;
        g = v15;
        b = v16;
        break;
    case 2:
        r = v16;
        g = v15;
        b = v17;
        break;
    case 3:
        r = v16;
        g = v10;
        b = v15;
        break;
    case 4:
        r = v17;
        g = v16;
        b = v15;
        break;
    case 5:
        r = v15;
        g = v16;
        b = v10;
        break;
    default:
        break;
    }

    dest[0] = r;
    dest[1] = g;
    dest[2] = b;
    dest[3] = a;

    if (bChangeColor && gBlacklistIndicators)
    {
        dest[0] *= gVisibility;
        dest[1] *= gVisibility;
        dest[2] *= gVisibility;
        dest[3] *= gVisibility;
    }

    return dest;
}

struct Screen
{
    int32_t Width;
    int32_t Height;
    float fWidth;
    float fHeight;
    float fHudOffset;
    float fAspectRatio;
    float fHudScaleX;
    float fHudScaleX2;
    float fTextScaleX;
    int32_t FilmstripScaleX;
    int32_t FilmstripOffset;
    uint32_t pFilmstripTex;
    std::filesystem::path szLoadscPath;
} Screen;

void InitLL()
{
    auto pattern = hook::pattern("74 ? 68 ? ? ? ? 53 FF D7");
    injector::MakeNOP(pattern.get_first(), 2, true);
}

void Init()
{
    CIniReader iniReader("");
    Screen.Width = iniReader.ReadInteger("MAIN", "ResX", 0);
    Screen.Height = iniReader.ReadInteger("MAIN", "ResY", 0);
    bool bForceLL = iniReader.ReadInteger("MAIN", "ForceLL", 1) != 0;
    auto nFPSLimit = iniReader.ReadInteger("MISC", "FPSLimit", 1000);
    Screen.szLoadscPath = iniReader.GetIniPath();
    Screen.szLoadscPath.replace_extension(".png");
    gBlacklistIndicators = iniReader.ReadInteger("BONUS", "BlacklistIndicators", 0);
    gColor = iniReader.ReadInteger("BONUS", "GogglesLightColor", 0);

    if (!Screen.Width || !Screen.Height)
        std::tie(Screen.Width, Screen.Height) = GetDesktopRes();

    char UserIni[MAX_PATH];
    GetModuleFileNameA(GetModuleHandle(NULL), UserIni, (sizeof(UserIni)));
    *strrchr(UserIni, '\\') = '\0';
    strcat(UserIni, "\\SplinterCell4.ini");

    CIniReader iniWriter(UserIni);
    char ResX[20];
    char ResY[20];
    _snprintf(ResX, 20, "%d", Screen.Width);
    _snprintf(ResY, 20, "%d", Screen.Height);
    iniWriter.WriteString("WinDrv.WindowsClient", "WindowedViewportX", ResX);
    iniWriter.WriteString("WinDrv.WindowsClient", "WindowedViewportY", ResY);
    iniWriter.WriteString("WinDrv.WindowsClient", "FullscreenViewportX", ResX);
    iniWriter.WriteString("WinDrv.WindowsClient", "FullscreenViewportY", ResY);

    if (bForceLL)
        CallbackHandler::RegisterCallback(InitLL, hook::pattern("74 ? 68 ? ? ? ? 53 FF D7").count_hint(1).empty(), 0x1100);

    if (nFPSLimit)
    {
        static float fFPSLimit = 1.0f / static_cast<float>(nFPSLimit);
        auto pattern = hook::pattern("A1 ? ? ? ? 8B 0D ? ? ? ? 89 45 DC 89 4D C4");
        injector::WriteMemory(pattern.get_first(1), &fFPSLimit, true);
    }

    //for test only (steam version)
    //CallbackHandler::RegisterCallback([]()
    //{
    //	auto pattern = hook::pattern("89 85 D8 61 00 00");
    //	struct StartupHook
    //	{
    //		void operator()(injector::reg_pack& regs)
    //		{
    //			*(uint32_t*)(regs.ebp + 0x61D8) = regs.eax;
    //			MessageBox(0, 0, L"test", 0);
    //		}
    //	}; injector::MakeInline<StartupHook>(pattern.get_first(0), pattern.get_first(6));
    //}, hook::pattern("89 85 D8 61 00 00").count_hint(1).empty(), 0x1100);
}

void InitD3DDrv()
{
    CIniReader iniReader("");
    static auto nShadowMapResolution = iniReader.ReadInteger("GRAPHICS", "ShadowMapResolution", 0);
    static auto nGlowResolution = iniReader.ReadInteger("GRAPHICS", "GlowResolution", 0);

    //https://github.com/ThirteenAG/WidescreenFixesPack/issues/725#issuecomment-612613927
    if (nShadowMapResolution)
    {
        //causes bugs #1251

        if (nShadowMapResolution > 8192)
            nShadowMapResolution = 8192;

        auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 00 04 00 00 C3 ? ? ? ? ? ? ? ? ? ? B8 00 04 00 00 C3 ? ? ? ? ? ? ? ? ? ? B8 00 02 00 00 C3 ? ? ? ? ? ? ? ? ? ? B8 00 02 00 00 C3");
        if (!pattern.empty())
        {
            auto range = hook::range_pattern((uintptr_t)pattern.get_first(0), (uintptr_t)pattern.get_first(54), "B8 ? ? ? ? C3");
            range.for_each_result([&](hook::pattern_match match) {
                injector::WriteMemory(match.get<void>(1), nShadowMapResolution, true);
            });
        }

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "0F B6 8E ? ? ? ? B8");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(3), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "6A 00 B8 ? ? ? ? D3 F8 8B 8E ? ? ? ? 8B 11");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(3), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "6A 02 B8");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(3), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 ? ? ? ? D3 F8 8D 8E ? ? ? ? 51 8B 8E");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(1), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 ? ? ? ? D3 F8 8B 8E ? ? ? ? 8B 11 6A 01 50 50 51 FF 52 5C 0F B6 8E");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(1), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 ? ? ? ? D3 F8 8B 8E ? ? ? ? 8B 09 6A 50");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(1), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 ? ? ? ? D3 F8 8D 8E ? ? ? ? 51 6A 00");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(1), nShadowMapResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 ? ? ? ? EB 11");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(1), nShadowMapResolution, true);
    }

    if (nGlowResolution)
    {
        if (nGlowResolution > 2048)
            nGlowResolution = 2048;

        static injector::hook_back<void(__fastcall*)(void* _this, int edx, int a2, int a3, int a4, int a5, int a6)> sub_100B9D10;
        auto sub_100B9D10_hook = [](void* _this, int edx, int a2, int a3, int a4, int a5, int a6)
        {
            a3 = nGlowResolution;
            a4 = nGlowResolution;
            return sub_100B9D10.fun(_this, edx, a2, a3, a4, a5, a6);
        };

        const char* xrefs_arr[] = {
            "E8 ? ? ? ? EB 02 33 C0 89 86 ? ? ? ? 8B 0D",
            "E8 ? ? ? ? EB 02 33 C0 89 86 ? ? ? ? A1 ? ? ? ? 8B 08 8B 11 68 ? ? ? ? 6A 10 89 5C 24 3C FF 12 89 44 24 40 85 C0 C7 44 24 ? ? ? ? ? 74 12 6A 00 6A 15 6A 40",
            "E8 ? ? ? ? EB 02 33 C0 89 86 ? ? ? ? A1 ? ? ? ? 8B 08 8B 11 68 ? ? ? ? 6A 10 89 5C 24 3C FF 12 89 44 24 40 85 C0 C7 44 24 ? ? ? ? ? 74 12 6A 00 6A 15 6A 20",
            "E8 ? ? ? ? EB 02 33 C0 89 86 ? ? ? ? A1 ? ? ? ? 8B 08 8B 11 68 ? ? ? ? 6A 10 89 5C 24 3C FF 12 89 44 24 40 85 C0 C7 44 24 ? ? ? ? ? 74 12 6A 00 6A 15 6A 10",
            "E8 ? ? ? ? EB 02 33 C0 89 86 ? ? ? ? A1 ? ? ? ? 8B 08 8B 11 68 ? ? ? ? 6A 10 89 5C 24 3C FF 12 89 44 24 40 85 C0 C7 44 24 ? ? ? ? ? 74 12 6A 00 6A 15 6A 08",
        };

        for (auto& xref : xrefs_arr)
        {
            auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), xref);
            if (!pattern.empty())
                sub_100B9D10.fun = injector::MakeCALL(pattern.get_first(), static_cast<void(__fastcall*)(void* _this, int edx, int a2, int a3, int a4, int a5, int a6)>(sub_100B9D10_hook), true).get();
        }
        
        auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? 8B 08");
        if (!pattern.empty())
        {
            injector::WriteMemory<float>(pattern.get_first(1),  1.003910000f, true);
            injector::WriteMemory<float>(pattern.get_first(6),  0.003906250f, true);
            injector::WriteMemory<float>(pattern.get_first(23), 1.001950000f, true);
            injector::WriteMemory<float>(pattern.get_first(28), 0.001953120f, true);
            injector::WriteMemory<float>(pattern.get_first(43), 1.000980000f, true);
            injector::WriteMemory<float>(pattern.get_first(48), 0.000976562f, true);
            injector::WriteMemory<float>(pattern.get_first(63), 1.000490000f, true);
            injector::WriteMemory<float>(pattern.get_first(68), 0.000488281f, true);
        }

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "68 ? ? ? ? 68 ? ? ? ? 50 55");
        if (!pattern.empty())
        {
            injector::WriteMemory(pattern.get_first(1), nGlowResolution, true);
            injector::WriteMemory(pattern.get_first(6), nGlowResolution, true);
        }

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "68 ? ? ? ? 68 ? ? ? ? 51 50");
        if (!pattern.empty())
        {
            injector::WriteMemory(pattern.get_first(1), nGlowResolution, true);
            injector::WriteMemory(pattern.get_first(6), nGlowResolution, true);
        }

        uint8_t asm_code[] = {
            0x68, 0xCC, 0xCC, 0xCC, 0xCC,                             //push    40h
            0x68, 0xCC, 0xCC, 0xCC, 0xCC,                             //push    40h
            0x51,                                                     //push    ecx
            0x50,                                                     //push    eax
            0x8B, 0xCE,                                               //mov     ecx, esi
            0xFF, 0x92, 0xD0, 0x04, 0x00, 0x00,                       //call    dword ptr[edx + 4D0h]
        };

        const char* xrefs_arr2[] = {
            "6A 40 6A 40 51 50 8B CE FF 92",
            "6A 20 6A 20 51 50 8B CE FF 92",
            "6A 10 6A 10 51 50 8B CE FF 92",
        };

        for (auto& xref : xrefs_arr2)
        {
            auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), xref);
            if (!pattern.empty())
            {
                injector::MakeNOP(pattern.get_first(), 31);
                injector::WriteMemoryRaw(pattern.get_first(), asm_code, sizeof(asm_code), true);
                injector::WriteMemory(pattern.get_first(1), nGlowResolution, true);
                injector::WriteMemory(pattern.get_first(6), nGlowResolution, true);
            }
        }

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 44 24 ? ? ? ? ? D9 5C 24 0C");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(4), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "89 54 24 3C C7 44 24");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(8), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 44 24 ? ? ? ? ? E8 ? ? ? ? 8B 10 F3 0F 10 05 ? ? ? ? 8B 86 ? ? ? ? 8B 88");
        if (!pattern.empty())
        {
            injector::WriteMemory(pattern.get(0).get<void>(4), nGlowResolution, true);
            injector::WriteMemory(pattern.get(2).get<void>(4), nGlowResolution, true);
        }

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 84 24 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 10 F3 0F 10 05 ? ? ? ? 8B 86 ? ? ? ? 8B 88");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get(0).get<void>(7), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 84 24 ? ? ? ? ? ? ? ? E8 ? ? ? ? F3 0F 10 05 ? ? ? ? 8B 10 6A 01");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(7), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 88 ? ? ? ? C7 44 24 ? ? ? ? ? E8");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(10), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "89 54 24 40 C7 44 24");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(8), nGlowResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 84 24 ? ? ? ? ? ? ? ? E8 ? ? ? ? F3 0F 10 05 ? ? ? ? 8B 10 F3 0F 11 84 24");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(7), nGlowResolution, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "C7 84 24 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 10 8B 84 24 ? ? ? ? 8B 8C 24");
        if (!pattern.empty())
            injector::WriteMemory(pattern.get_first(7), nGlowResolution, true);
        
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B9 ? ? ? ? FF 25");
        if (!pattern.empty())
        {
            uint8_t asm_code[] = {
                0x6A, 0x15, 0x68, 0x00, 0x02, 0x00, 0x00, 0x68, 0x00, 0x02, 0x00, 0x00,
                0xE9, 0xD4, 0xC6, 0xF6, 0xFF, 0x6A, 0x15, 0x68, 0x00, 0x01, 0x00, 0x00,
                0x68, 0x00, 0x01, 0x00, 0x00, 0xE9, 0x03, 0xC7, 0xF6, 0xFF, 0x6A, 0x15,
                0x68, 0x80, 0x00, 0x00, 0x00, 0x68, 0x80, 0x00, 0x00, 0x00, 0xE9, 0x32,
                0xC7, 0xF6, 0xFF, 0xC7, 0x44, 0xE4, 0x6C, 0x00, 0x00, 0x00, 0x45, 0xC7,
                0x44, 0xE4, 0x70, 0x00, 0x00, 0x00, 0x45, 0xC7, 0x84, 0xE4, 0x90, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x80, 0x44, 0xC7, 0x84, 0xE4, 0x94, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x80, 0x44, 0xC7, 0x84, 0xE4, 0xB4, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x44, 0xC7, 0x84, 0xE4, 0xB8, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x44, 0xC7, 0x84, 0xE4, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x80, 0x43, 0xC7, 0x84, 0xE4, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
                0x43, 0xE8, 0x3B, 0xB6, 0xF8, 0xFF, 0xE9, 0x85, 0xE0, 0xF8, 0xFF, 0xC7,
                0x44, 0xE4, 0x70, 0x00, 0x00, 0x00, 0x45, 0xC7, 0x44, 0xE4, 0x74, 0x00,
                0x00, 0x00, 0x45, 0xC7, 0x84, 0xE4, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x80, 0x44, 0xC7, 0x84, 0xE4, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
                0x44, 0xC7, 0x84, 0xE4, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44,
                0xC7, 0x84, 0xE4, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0xC7,
                0x84, 0xE4, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x43, 0xC7, 0x84,
                0xE4, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x43, 0xE8, 0xDF, 0xB5,
                0xF8, 0xFF, 0xE9, 0x57, 0xE2, 0xF9, 0xFF, 0x68, 0x00, 0x02, 0x00, 0x00,
                0x68, 0x00, 0x02, 0x00, 0x00, 0x51, 0xE9, 0xF9, 0xCE, 0xF8, 0xFF, 0x68,
                0x00, 0x01, 0x00, 0x00, 0x68, 0x00, 0x01, 0x00, 0x00, 0x51, 0xE9, 0x9F,
                0xCF, 0xF8, 0xFF, 0x68, 0x80, 0x00, 0x00, 0x00, 0x68, 0x80, 0x00, 0x00,
                0x00, 0x51, 0xE9, 0x3A, 0xD0, 0xF8, 0xFF
            };

            uint8_t asm_jmp[] = {
                0xE9, 0x1F, 0x1F, 0x07, 0x00
            };
            
            uint8_t asm_jmp2[] = {
                0xE9, 0x4D, 0x1D, 0x06, 0x00
            };
            injector::WriteMemoryRaw(pattern.get_first(11), asm_code, sizeof(asm_code), true);
            
            pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "E8 ? ? ? ? E8 ? ? ? ? 8B 16");
            injector::WriteMemoryRaw(pattern.get_first(0), asm_jmp, sizeof(asm_jmp), true);

            pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "F3 0F 11 84 24 ? ? ? ? E8 ? ? ? ? 5F");
            injector::WriteMemoryRaw(pattern.get_first(9), asm_jmp2, sizeof(asm_jmp2), true);
            
            uint8_t flts[] = {
                0x51, 0x68, 0x00, 0x40, 0x80, 0x3F, 0x68, 0x00, 0x00, 0x00, 0x3B, 0x68, 0x00, 0x00, 0x80, 0xBF, 0x68, 0x00, 0x00, 0x80,
                0x3F, 0x68, 0x00, 0x20, 0x80, 0x3F, 0x68, 0x00, 0x00, 0x80, 0x3A, 0x68, 0x00, 0x00, 0x80, 0xBF, 0x68, 0x00, 0x00, 0x80,
                0x3F, 0x68, 0x00, 0x10, 0x80, 0x3F, 0x68, 0x00, 0x00, 0x00, 0x3A
            };
            
            pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "51 68 00 00 82 3F 68 00 00 80 3C 68 00 00 80 BF 68 00 00 80 3F 68 00 00 81 3F 68 00 00 00 3C 68 00 00 80 BF 68 00 00 80 3F 68 00 80 80 3F 68 00 00 80 3B");
            injector::WriteMemoryRaw(pattern.get_first(0), flts, sizeof(flts), true);
        }
    }
    
    //Minimap aka FilmStrip
    auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 4B 04 03 C9 85 C9 89 4C 24 10"); //?RenderFilmstrip@UD3DRenderDevice@@UAE_NXZ + 0x38C
    struct RenderFilmstrip_Hook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.ecx = Screen.FilmstripScaleX + Screen.FilmstripScaleX;
        }
    }; injector::MakeInline<RenderFilmstrip_Hook>(pattern.get_first(0));

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "03 D2 85 D2 89 54 24 10 DB 44 24 10"); //?RenderFilmstrip@UD3DRenderDevice@@UAE_NXZ + 0x350
    struct RenderFilmstrip_Hook2
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = Screen.FilmstripOffset;
            *(int32_t*)(regs.esp + 0x10) = regs.edx;
        }
    }; injector::MakeInline<RenderFilmstrip_Hook2>(pattern.get_first(0), pattern.get_first(6));
    injector::WriteMemory<uint16_t>(pattern.get_first(6), 0xD285, true);     //test    edx, edx

    //SetRes
    static auto pPresentParams = *hook::module_pattern(GetModuleHandle(L"D3DDrv"), "BF ? ? ? ? F3 AB 8B 0D ? ? ? ? 39 01 8B 4D").get_first<D3DPRESENT_PARAMETERS*>(1);
    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "B8 01 00 00 00 64 89 0D 00 00 00 00 5B 8B E5 5D C2 10 00");
    struct SetResHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = 1;

            Screen.Width = pPresentParams->BackBufferWidth;
            Screen.Height = pPresentParams->BackBufferHeight;
            Screen.fWidth = static_cast<float>(Screen.Width);
            Screen.fHeight = static_cast<float>(Screen.Height);
            Screen.fAspectRatio = (Screen.fWidth / Screen.fHeight);
            Screen.fHudScaleX = 1.0f / (((4.0f / 3.0f)) / (Screen.fAspectRatio));
            Screen.fHudScaleX2 = 2.0f / (600.0f * Screen.fAspectRatio);
            Screen.fTextScaleX = 600.0f * Screen.fAspectRatio;
            Screen.fHudOffset = ((4.0f / 3.0f) / Screen.fAspectRatio);
            Screen.FilmstripScaleX = static_cast<int32_t>(Screen.fWidth / (1280.0f / (368.0 * ((4.0 / 3.0) / (Screen.fAspectRatio)))));
            Screen.FilmstripOffset = static_cast<int32_t>((((Screen.fWidth / 2.0f) - ((Screen.fHeight * (4.0f / 3.0f)) / 2.0f)) * 2.0f) + ((float)Screen.FilmstripScaleX / 5.25f));

            CIniReader iniReader("");
            auto[DesktopResW, DesktopResH] = GetDesktopRes();
            if (Screen.Width != DesktopResW || Screen.Height != DesktopResH)
            {
                iniReader.WriteInteger("MAIN", "ResX", Screen.Width);
                iniReader.WriteInteger("MAIN", "ResY", Screen.Height);
            }
            else
            {
                iniReader.WriteInteger("MAIN", "ResX", 0);
                iniReader.WriteInteger("MAIN", "ResY", 0);
            }

            if (pPresentParams->Windowed)
            {
                tagRECT rect;
                rect.left = (LONG)(((float)DesktopResW / 2.0f) - (Screen.fWidth / 2.0f));
                rect.top = (LONG)(((float)DesktopResH / 2.0f) - (Screen.fHeight / 2.0f));
                rect.right = (LONG)Screen.Width;
                rect.bottom = (LONG)Screen.Height;
                SetWindowLong(pPresentParams->hDeviceWindow, GWL_STYLE, GetWindowLong(pPresentParams->hDeviceWindow, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(pPresentParams->hDeviceWindow, NULL, rect.left, rect.top, rect.right, rect.bottom, SWP_NOACTIVATE | SWP_NOZORDER);
                SetForegroundWindow(pPresentParams->hDeviceWindow);
                SetCursor(NULL);
            }
        }
    }; injector::MakeInline<SetResHook>(pattern.get_first(0));

    //ScopeLens
    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 80 ? ? ? ? 8B 08 6A 14 8D 54 24 70 52 6A 02 6A 05");
    struct ScopeLensHook1
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.eax + 0x5F30);

            if (Screen.fAspectRatio > (4.0f / 3.0f))
            {
                auto pVertexStreamZeroData = regs.esp + 0x6C;
                *(float*)(pVertexStreamZeroData + 0x00) /= (Screen.fAspectRatio / (16.0f / 9.0f)) / (((4.0f / 3.0f)) / (Screen.fAspectRatio));
                *(float*)(pVertexStreamZeroData + 0x14) /= (Screen.fAspectRatio / (16.0f / 9.0f)) / (((4.0f / 3.0f)) / (Screen.fAspectRatio));
                *(float*)(pVertexStreamZeroData + 0x28) /= (Screen.fAspectRatio / (16.0f / 9.0f)) / (((4.0f / 3.0f)) / (Screen.fAspectRatio));
                *(float*)(pVertexStreamZeroData + 0x3C) /= (Screen.fAspectRatio / (16.0f / 9.0f)) / (((4.0f / 3.0f)) / (Screen.fAspectRatio));

                *(float*)(pVertexStreamZeroData + 0x04) /= (4.0f / 3.0f);
                *(float*)(pVertexStreamZeroData + 0x18) /= (4.0f / 3.0f);
                *(float*)(pVertexStreamZeroData + 0x2C) /= (4.0f / 3.0f);
                *(float*)(pVertexStreamZeroData + 0x40) /= (4.0f / 3.0f);
            }
        }
    }; injector::MakeInline<ScopeLensHook1>(pattern.get_first(0), pattern.get_first(6));

    //crashfix
    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "6A 04 8B CE F3 0F 11 44 24"); //
    struct RenderFilmstrip_Hook3
    {
        void operator()(injector::reg_pack& regs)
        {
            if (regs.edi)
            {
                Screen.pFilmstripTex = regs.edi;
                auto pTex = (IDirect3DTexture9*)regs.edi;
                auto type = pTex->GetType();

                if (type == 0) // check if ptr valid so it doesn't crash
                {
                    regs.edi = 0;
                    *(uint32_t*)(regs.ebx + 0x3C) = regs.edi;
                }
            }
            static float _xmm0 = 0.0f;
            _asm movss   dword ptr[_xmm0], xmm0
            *(float*)(regs.esp + 0x18) = _xmm0;
        }
    }; injector::MakeInline<RenderFilmstrip_Hook3>(pattern.get_first(4), pattern.get_first(10));

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 46 2C 3B C7 74 1D 39 7E 4C 50"); //
    static auto GMalloc = *pattern.get_first<uint32_t>(22);
    struct FlushHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x2C);
            if (regs.eax && regs.eax != Screen.pFilmstripTex)
            {
                regs.eax = *(uint32_t*)(regs.esi + 0x2C);
                if (*(uint32_t*)(regs.esi + 0x4C))
                    (*(void(__stdcall**)(int))(*(uint32_t*)GMalloc + 8))(regs.eax);
                else
                    (*(void(__stdcall**)(int))(*(uint32_t*)regs.eax + 8))(regs.eax);
                *(uint32_t*)(regs.esi + 0x2C) = 0;
            }
        }
    }; injector::MakeInline<FlushHook>(pattern.get_first(0), pattern.get_first(36));

    //loadscreen
    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "89 BE ? ? ? ? E8 ? ? ? ? 8B 86 ? ? ? ? 8B 08 57 6A 00 50 FF 91 ? ? ? ? 6A 04 8B CB E8 ? ? ? ? 8B 6C 24 30 F6 45 54 01 74 48 83 BE"); //
    struct LoadscHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if (regs.edi)
            {
                auto pTex = (IDirect3DTexture9*)regs.edi;
                if (pTex->GetType() == D3DRTYPE_TEXTURE)
                {
                    D3DSURFACE_DESC pDesc;
                    pTex->GetLevelDesc(0, &pDesc);
                    if (pDesc.Width == 2048 && pDesc.Height == 1024 && pDesc.Format == D3DFMT_DXT5 && pDesc.Usage == 0)
                    {
                        static LPDIRECT3DTEXTURE9 pTexLoadscreenCustom = nullptr;
                        if (!pTexLoadscreenCustom)
                        {
                            IDirect3DDevice9* ppDevice = nullptr;
                            pTex->GetDevice(&ppDevice);
                            if (D3DXCreateTextureFromFileW(ppDevice, (LPCWSTR)(Screen.szLoadscPath.u16string().c_str()), &pTexLoadscreenCustom) == D3D_OK)
                                regs.edi = (uint32_t)pTexLoadscreenCustom;
                        }
                        else
                            regs.edi = (uint32_t)pTexLoadscreenCustom;
                    }
                }
            }
            *(uint32_t*)(regs.esi + 0x6EB0) = regs.edi;
        }
    }; injector::MakeInline<LoadscHook>(pattern.get_first(0), pattern.get_first(6));

    //Enhanced night vision NaN workaround
    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "F3 A5 8B 90 30 02 00 00");
    injector::MakeNOP(pattern.get_first(0), 2, true);

    //Goggles Light Color
    if (gColor || gBlacklistIndicators)
    {
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "FF 15 ? ? ? ? 83 C4 10 8D AE ? ? ? ? 6A 01 8B CD 8B D8 E8");
        injector::MakeNOP(pattern.get(0).get<void>(0), 6, true);
        injector::MakeCALL(pattern.get(0).get<void>(0), FGetHSV, true);

        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "FF 15 ? ? ? ? 8B 6D 5C 0F 57 C0 83 C4 10 85 ED F3 0F 11 44 24 ? 0F 84 ? ? ? ? 8B 44 24 10 F3 0F 10 40");
        injector::MakeNOP(pattern.get(0).get<void>(0), 6, true);
        injector::MakeCALL(pattern.get(0).get<void>(0), FGetHSV, true);
    }

    {
        pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B D1 F7 DA");
        struct FullScreen_RefreshRateInHzHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.edx = regs.ecx != 0 ? 1000 : 0;
            }
        }; injector::MakeInline<FullScreen_RefreshRateInHzHook>(pattern.get_first(0), pattern.get_first(9));
    }
}

void InitEngine()
{
    CIniReader iniReader("");
    bool bSingleCoreAffinity = iniReader.ReadInteger("MAIN", "SingleCoreAffinity", 1);

    static bool bIsOPSAT = false;
    static bool bIsVideoPlaying = false;
    static auto GIsWideScreen = *hook::module_pattern(GetModuleHandle(L"Engine"), "8B 0D ? ? ? ? F3 0F 59 D9 F3 0F 10 0D ? ? ? ? F3 0F 5C D8 F3 0F 58 DC").get_first<uint8_t*>(2);
    static auto GIsSameFrontBufferOnNormalTV = *hook::module_pattern(GetModuleHandle(L"Engine"), "8B 15 ? ? ? ? 83 3A 00 74 58 F3 0F 10 88").get_first<uint8_t*>(2);
    auto pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "83 39 00 75 63 8B 15 ? ? ? ? 83 3A 00"); //
    static auto dword_109E09F0 = *pattern.get_first<float*>(60);
    static auto dword_109E09F4 = dword_109E09F0 + 1;
    static auto dword_109E09F8 = dword_109E09F0 + 2;
    static auto dword_109E09FC = dword_109E09F0 + 3;
    struct ImageUnrealDrawHook
    {
        void operator()(injector::reg_pack& regs)
        {
            //auto x = *(float*)&dword_109E09F0[regs.eax / 4];
            //auto y = *(float*)&dword_109E09F4[regs.eax / 4];
            auto w = *(float*)&dword_109E09F8[regs.eax / 4];
            auto h = *(float*)&dword_109E09FC[regs.eax / 4];

            if (!*GIsWideScreen && *GIsSameFrontBufferOnNormalTV)
            {
                *(float*)&dword_109E09F0[regs.eax / 4] *= 0.75f;
                *(float*)&dword_109E09F4[regs.eax / 4] *= 0.75f;
                *(float*)&dword_109E09F8[regs.eax / 4] *= 0.75f;
                *(float*)&dword_109E09FC[regs.eax / 4] *= 0.75f;
            }
            else
            {
                if (bIsOPSAT && bIsVideoPlaying)
                {
                    *(float*)&dword_109E09F0[regs.eax / 4] *= (4.0f / 3.0f);
                    *(float*)&dword_109E09F4[regs.eax / 4] *= (4.0f / 3.0f);
                    *(float*)&dword_109E09F8[regs.eax / 4] *= (4.0f / 3.0f);
                    *(float*)&dword_109E09FC[regs.eax / 4] *= (4.0f / 3.0f);
                }
            }

            if ((w == 1275.0f && h == 637.0f) || (w == 800.0f)) // loadscreen and fullscreen images
            {
                *(float*)&dword_109E09F0[regs.eax / 4] *= Screen.fHudScaleX;
                *(float*)&dword_109E09F8[regs.eax / 4] *= Screen.fHudScaleX;

                if (w == 1275.0f && h == 637.0f)
                {
                    *(float*)&dword_109E09F4[regs.eax / 4] *= Screen.fHudScaleX;
                    *(float*)&dword_109E09FC[regs.eax / 4] *= Screen.fHudScaleX;
                }
            }
        }
    }; injector::MakeInline<ImageUnrealDrawHook>(pattern.get_first(0), pattern.get_first(104));

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "D9 05 ? ? ? ? 83 C4 08 D8 C9 D9 5C 24 24");
    injector::WriteMemory(pattern.get_first(2), &Screen.fHudScaleX2, true);
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "D9 05 ? ? ? ? D8 4C 24 10 D9 5C 24 28");
    injector::WriteMemory(pattern.get_first(2), &Screen.fHudScaleX2, true);
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 10 0D ? ? ? ? F3 0F 11 44 24 ? F3 0F 11 44 24 ? F3 0F 10 05 ? ? ? ? F3 0F 11 4C 24 ? F3 0F 11 44 24");
    injector::WriteMemory(pattern.get_first(4), &Screen.fHudScaleX2, true);
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 10 0D ? ? ? ? F3 0F 10 15 ? ? ? ? 2B D1 F3 0F 2A C2");
    injector::WriteMemory(pattern.get_first(4), &Screen.fHudScaleX2, true);
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 5C 1D ? ? ? ? 0F 28 FD F3 0F 59 FA"); //0x103069A5 + 0x4
    injector::WriteMemory(pattern.get_first(4), &Screen.fHudOffset, true);

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 5C D8 F3 0F 58 DC"); //0x1030503C
    struct HUDPosHook
    {
        void operator()(injector::reg_pack& regs)
        {
            static const float f1_0 = 1.0f;
            _asm
            {
                movss xmm0, Screen.fHudOffset
                subss xmm3, xmm0
                addss xmm3, xmm4
                movss xmm0, f1_0
            }
        }
    }; injector::MakeInline<HUDPosHook>(pattern.get_first(0), pattern.get_first(8));

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 5E CA F3 0F 11 4C 24 14 83 C4 04"); //0x1030678D
    struct TextHook
    {
        void operator()(injector::reg_pack& regs)
        {
            auto regs_ecx = regs.ecx;
            _asm
            {
                cvtsi2ss xmm2, regs_ecx
                movss xmm2, Screen.fTextScaleX
                divss xmm1, xmm2
            }
        }
    }; injector::MakeInline<TextHook>(pattern.get_first(-4), pattern.get_first(4));

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "F3 0F 5C 1D ? ? ? ? 0F 28 FD F3 0F 59 FA"); //0x103069A5 + 0x4
    injector::WriteMemory(pattern.get_first(4), &Screen.fHudOffset, true);

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "8B 0D ? ? ? ? 83 39 00 89 54 24 20 75 3D 8B 15 ? ? ? ? 83 3A 00");
    struct TextHook2
    {
        void operator()(injector::reg_pack& regs)
        {
            auto f3 = 0.0f;
            auto f4 = *(float*)(regs.esp + 0x24);
            auto f5 = 0.0f;
            auto f6 = 0.0f;
            _asm { movss dword ptr ds : f3, xmm3}
            _asm { movss dword ptr ds : f5, xmm5}
            _asm { movss dword ptr ds : f6, xmm6}

            if (!*GIsWideScreen && *GIsSameFrontBufferOnNormalTV)
            {
                *(float *)(regs.ebp + 0x00) = f3 * 0.75f;
                *(float *)(regs.ebp + 0x04) = f4 * 0.75f;
                *(float *)(regs.ebp + 0x08) = f5 * 0.75f;
                *(float *)(regs.ebp + 0x0C) = f6 * 0.75f;
            }
            else
            {
                if (bIsOPSAT && bIsVideoPlaying)
                {
                    *(float *)(regs.ebp + 0x00) = f3 * (4.0f / 3.0f);
                    *(float *)(regs.ebp + 0x04) = f4 * (4.0f / 3.0f);
                    *(float *)(regs.ebp + 0x08) = f5 * (4.0f / 3.0f);
                    *(float *)(regs.ebp + 0x0C) = f6 * (4.0f / 3.0f);
                }
            }
        }
    }; injector::MakeInline<TextHook2>(pattern.get_first(0), pattern.get_first(76));

    //OPSAT and computer videos (scale text and hud like x360 version)
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "8B 86 ? ? ? ? 83 C4 10 85 C0 74 49 83 78 64 00"); //0x10515086
    struct UCanvasOpenVideoHook
    {
        void operator()(injector::reg_pack& regs)
        {
            static auto getVideoPath = [](const std::string_view& str, char beg, char end) -> std::string
            {
                std::size_t begPos;
                if ((begPos = str.find(beg)) != std::string_view::npos)
                {
                    std::size_t endPos;
                    if ((endPos = str.find(end, begPos)) != std::string_view::npos && endPos != begPos + 1)
                        return std::string(str).substr(begPos + 1, endPos - begPos - 1);
                }

                return std::string();
            };
            bIsVideoPlaying = true;
            regs.eax = *(uint32_t*)(regs.esi + 0x80);
            auto str = getVideoPath(std::string_view(*(char**)(regs.esp + 0x0C)), '\\', '.');
            if (iequals(str, "computer_hq_1") || iequals(str, "computer_hq_2") || iequals(str, "computer_hq_3") ||
                iequals(str, "computer_mission_1") || iequals(str, "computer_mission_2") || starts_with(str.c_str(), "opsat", false))
                bIsOPSAT = true;
            else
                bIsOPSAT = false;
        }
    }; injector::MakeInline<UCanvasOpenVideoHook>(pattern.get_first(0), pattern.get_first(6));

    //
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "8B 86 ? ? ? ? 85 C0 74 12 83 78 64 00 74 0C"); //0x10515110
    struct UCanvasCloseVideoHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x80);
            bIsVideoPlaying = false;
        }
    }; injector::MakeInline<UCanvasCloseVideoHook>(pattern.get_first(0), pattern.get_first(6));

    //FOV
    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "8B 91 28 06 00 00 52 8B"); //?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z  10530BD7
    struct UGameEngine_Draw_Hook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(float*)&regs.edx = AdjustFOV(*(float*)(regs.ecx + 0x628), Screen.fAspectRatio);
        }
    }; injector::MakeInline<UGameEngine_Draw_Hook>(pattern.get_first(0), pattern.get_first(6));

    if (bSingleCoreAffinity)
    {
        AffinityChanges::Init();
        AffinityChanges::RedirectImports(GetModuleHandle(L"Engine"));
    }
}

void InitWindow()
{
    //icon fix
    auto RegisterClassExWProxy = [](WNDCLASSEXW* lpwcx) -> ATOM {
        lpwcx->hIcon = CreateIconFromResourceICO(IDR_SCDAICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
        lpwcx->hIconSm = CreateIconFromResourceICO(IDR_SCDAICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
        return RegisterClassExW(lpwcx);
    };

    auto RegisterClassExAProxy = [](WNDCLASSEXA* lpwcx) -> ATOM {
        lpwcx->hIcon = CreateIconFromResourceICO(IDR_SCDAICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
        lpwcx->hIconSm = CreateIconFromResourceICO(IDR_SCDAICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
        return RegisterClassExA(lpwcx);
    };

    auto pattern = hook::module_pattern(GetModuleHandle(L"Window"), "FF 15 ? ? ? ? 66 85 C0 0F 85 ? ? ? ? 68 F5 02 00 00");
    injector::WriteMemory(*pattern.get_first<void*>(2), static_cast<ATOM(WINAPI*)(WNDCLASSEXW*)>(RegisterClassExWProxy), true);
    pattern = hook::module_pattern(GetModuleHandle(L"Window"), "FF 15 ? ? ? ? 66 85 C0 75 18 68 03 03 00 00");
    injector::WriteMemory(*pattern.get_first<void*>(2), static_cast<ATOM(WINAPI*)(WNDCLASSEXA*)>(RegisterClassExAProxy), true);
}

void InitEchelonMenus()
{
    CIniReader iniReader("");
    bool bSkipIntro = iniReader.ReadInteger("MISC", "SkipIntro", 1);

    if (bSkipIntro)
    {
        auto pattern = hook::module_pattern(GetModuleHandle(L"EchelonMenus"), "75 0A C7 05 ? ? ? ? ? ? ? ? 8B 4D F4");
        injector::WriteMemory(pattern.get_first(8), 5, true);

#ifdef DEBUG
        {
            //uint8_t asm_code[13] = { 0 };
            //auto pattern = hook::module_pattern(GetModuleHandle(L"EchelonMenus"), "83 F8 05 0F 87");
            //injector::ReadMemoryRaw(pattern.get_first(3), asm_code, sizeof(asm_code), true);
            //injector::MakeNOP(pattern.get_first(3), 13);
            keybd_event(VK_RETURN, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
            keybd_event(VK_RETURN, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
            //injector::WriteMemoryRaw(pattern.get_first(3), asm_code, sizeof(asm_code), true);
        }
#endif
    }

    static std::vector<std::string> list;
    GetResolutionsList(list);
    std::swap(list[list.size() - 5], list[list.size() - 6]);
    list[list.size() - 5] = format("%dx%d", Screen.Width, Screen.Height);

    auto pattern = hook::module_pattern(GetModuleHandle(L"EchelonMenus"), "C6 45 FC 10 33 F6 83 FE 07");
    auto rpattern = hook::range_pattern((uintptr_t)pattern.get_first(-850), (uintptr_t)pattern.get_first(0), "C7 45 E8");
    for (size_t i = 0; i < rpattern.size(); ++i)
    {
        injector::WriteMemory(rpattern.get(i).get<uint32_t>(3), &list[list.size() - 1 - i][0], true);
    }

    static const auto wsz0 = 0;
    rpattern = hook::range_pattern((uintptr_t)pattern.get_first(-850), (uintptr_t)pattern.get_first(0), "8D ? E4 68");
    for (size_t i = 0; i < rpattern.size(); ++i)
    {
        injector::WriteMemory(rpattern.get(i).get<uint32_t>(4), &wsz0, true);
    }

    struct ResListHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.ebp - 4) = 16;
            regs.esi = 0;
            list.clear();
        }
    }; injector::MakeInline<ResListHook>(pattern.get_first(0), pattern.get_first(6));
}

void InitEchelon()
{
    CIniReader iniReader("");
    gBlacklistIndicators = iniReader.ReadInteger("BONUS", "BlacklistIndicators", 0);
    
    if (gBlacklistIndicators)
    {
        auto pattern = hook::module_pattern(GetModuleHandle(L"Echelon"), "8B 96 ? ? ? ? 8D 0C 80");
        struct BlacklistIndicatorsHook
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.edx = *(uint32_t*)(regs.esi + 0x15B8);
                regs.ecx = regs.eax + regs.eax * 4;
                
                if (regs.ecx == 0)
                {
                    auto fPlayerLuminosity = *(float*)(regs.edx + regs.ecx * 4 + 4);
                    if (fPlayerLuminosity > 120.0f) fPlayerLuminosity = 120.0f;

                    auto v = 120.0f - fPlayerLuminosity;
                    if (v <= 10.0f) v = 10.0f;
                    gVisibility = ((float)v / 120.0f);
                    if (gBlacklistIndicators == 2)
                        gVisibility = 1.0f - ((float)v / 120.0f);
                }
            }
        }; injector::MakeInline<BlacklistIndicatorsHook>(pattern.get_first(0), pattern.get_first(6));
    }
}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
        {
            CallbackHandler::RegisterCallback(Init);
            CallbackHandler::RegisterCallback(L"Window.dll", InitWindow);
            CallbackHandler::RegisterCallback(L"Engine.dll", InitEngine);
            CallbackHandler::RegisterCallback(L"D3DDrv.dll", InitD3DDrv);
            CallbackHandler::RegisterCallback(L"EchelonMenus.dll", InitEchelonMenus);
            CallbackHandler::RegisterCallback(L"Echelon.dll", InitEchelon);
        });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}