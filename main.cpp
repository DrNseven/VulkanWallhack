// Vulkan Hook
#include <Windows.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <fstream>
#include "vulkan/vulkan.h"
#include "minhook/include/MinHook.h"

#pragma comment(lib, "vulkan/vulkan-1.lib")


// --- Globals ---
int countnum = -1;
std::shared_mutex pipeMapMtx;
bool reversedDepth = false;

#include <fstream>
inline void Log(const char* fmt, ...) {
    char text[4096] = { 0 };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    std::ofstream logfile("log.txt", std::ios::app);
    if (logfile.is_open()) {
        logfile << text << std::endl;
    }
}

//Pipeline
struct PipelineSettings {
    uint32_t stride;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    VkCompareOp depthCompareOp;
};
// Maps to track state
std::unordered_map<VkPipeline, PipelineSettings> pipelineData;
std::unordered_map<VkCommandBuffer, uint32_t> cmdBufferToStride;


//Viewport
// Command Buffer State
struct CmdState {
    VkViewport currentViewport;
    uint32_t firstViewport;
    bool hasViewport = false;
};
std::unordered_map<VkCommandBuffer, CmdState> cmdStates;
std::shared_mutex statesMtx;


// --- Function Pointers ---
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetDeviceProcAddr)(VkDevice device, const char* pName);
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);

typedef VkResult(VKAPI_PTR* PFN_vkCreateGraphicsPipelines)(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
typedef void (VKAPI_PTR* PFN_vkCmdBindPipeline)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
typedef void (VKAPI_PTR* PFN_vkCmdSetViewport_Custom)(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*);

typedef void (VKAPI_PTR* PFN_vkCmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexed)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexedIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndirectCount)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexedIndirectCount)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t);

PFN_vkGetDeviceProcAddr pOriginalGetDeviceProcAddr = nullptr;
PFN_vkGetInstanceProcAddr pOriginalGetInstanceProcAddr = nullptr;

PFN_vkCreateGraphicsPipelines pOriginalCreateGraphicsPipelines = nullptr;
PFN_vkCmdBindPipeline         pOriginalCmdBindPipeline = nullptr;
PFN_vkCmdSetViewport_Custom pOriginalCmdSetViewport = nullptr;

PFN_vkCmdDraw pOriginalCmdDraw = nullptr;
PFN_vkCmdDrawIndexed pOriginalCmdDrawIndexed = nullptr;
PFN_vkCmdDrawIndirect pOriginalCmdDrawIndirect = nullptr;
PFN_vkCmdDrawIndexedIndirect pOriginalCmdDrawIndexedIndirect = nullptr;
PFN_vkCmdDrawIndirectCount pOriginalCmdDrawIndirectCount = nullptr;
PFN_vkCmdDrawIndexedIndirectCount pOriginalCmdDrawIndexedIndirectCount = nullptr;


// Dynamic State Setters (The missing link)
PFN_vkCmdSetDepthTestEnable  gp_vkCmdSetDepthTestEnable = nullptr;
PFN_vkCmdSetDepthWriteEnable gp_vkCmdSetDepthWriteEnable = nullptr;
PFN_vkCmdSetDepthCompareOp   gp_vkCmdSetDepthCompareOp = nullptr;

// --- Detours ---
bool gSupportsExtendedDynamicState = false;
VkResult VKAPI_CALL DetourCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache cache,
    uint32_t count,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("1");
        loggedOnce = true;
    }

    std::vector<VkGraphicsPipelineCreateInfo> modifiedInfos(pCreateInfos, pCreateInfos + count);
    std::vector<PipelineSettings> capturedSettings(count);

    // Must stay alive during vkCreateGraphicsPipelines
    std::vector<std::vector<VkDynamicState>> dynamicStatesArrays(count);
    std::vector<VkPipelineDynamicStateCreateInfo> dynamicStateStructs(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        auto& ci = modifiedInfos[i];

        // -----------------------------
        // Capture vertex stride
        // -----------------------------
        capturedSettings[i].stride = 0;

        if (ci.pVertexInputState &&
            ci.pVertexInputState->vertexBindingDescriptionCount > 0)
        {
            capturedSettings[i].stride =
                ci.pVertexInputState->pVertexBindingDescriptions[0].stride;
        }

        // -----------------------------
        // Capture depth settings
        // -----------------------------
        if (ci.pDepthStencilState)
        {
            capturedSettings[i].depthTestEnable =
                ci.pDepthStencilState->depthTestEnable;

            capturedSettings[i].depthWriteEnable =
                ci.pDepthStencilState->depthWriteEnable;

            capturedSettings[i].depthCompareOp =
                ci.pDepthStencilState->depthCompareOp;
        }
        else
        {
            capturedSettings[i].depthTestEnable = VK_FALSE;
            capturedSettings[i].depthWriteEnable = VK_FALSE;
            capturedSettings[i].depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }

        // -----------------------------
        // SAFETY CHECKS
        // -----------------------------

        // Only modify if depth exists AND the extension exists
        if (!ci.pDepthStencilState || !gSupportsExtendedDynamicState)
            continue;

        // If pipeline has dynamic state already, copy it
        if (ci.pDynamicState)
        {
            for (uint32_t j = 0; j < ci.pDynamicState->dynamicStateCount; ++j)
            {
                dynamicStatesArrays[i].push_back(
                    ci.pDynamicState->pDynamicStates[j]);
            }
        }

        // Helper to add dynamic states safely
        auto addState = [&](VkDynamicState state)
            {
                for (VkDynamicState existing : dynamicStatesArrays[i])
                {
                    if (existing == state)
                        return;
                }

                dynamicStatesArrays[i].push_back(state);
            };

        addState(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        addState(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        addState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);

        dynamicStateStructs[i].sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

        dynamicStateStructs[i].pNext =
            ci.pDynamicState ? ci.pDynamicState->pNext : nullptr;

        dynamicStateStructs[i].dynamicStateCount =
            (uint32_t)dynamicStatesArrays[i].size();

        dynamicStateStructs[i].pDynamicStates =
            dynamicStatesArrays[i].data();

        ci.pDynamicState = &dynamicStateStructs[i];
    }

    // Call original Vulkan function
    VkResult res = pOriginalCreateGraphicsPipelines(
        device,
        cache,
        count,
        modifiedInfos.data(),
        pAllocator,
        pPipelines);

    // Store captured pipeline settings
    if (res == VK_SUCCESS)
    {
        std::unique_lock<std::shared_mutex> lock(pipeMapMtx);

        for (uint32_t i = 0; i < count; ++i)
        {
            if (pPipelines[i] != VK_NULL_HANDLE)
            {
                pipelineData[pPipelines[i]] = capturedSettings[i];
            }
        }
    }

    return res;
}


void VKAPI_CALL DetourCmdBindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipeline pipeline) {
    pOriginalCmdBindPipeline(cmd, bindPoint, pipeline);

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("2");
        loggedOnce = true;
    }

    if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        PipelineSettings settings = { 0 };
        bool found = false;

        {
            std::shared_lock<std::shared_mutex> lock(pipeMapMtx);
            auto it = pipelineData.find(pipeline);
            if (it != pipelineData.end()) {
                settings = it->second;
                cmdBufferToStride[cmd] = settings.stride;
                found = true;
            }
        }

        // IMPORTANT: Because we made the pipeline dynamic, the game's original static settings 
        // are now IGNORED by the driver. We must manually apply the original state here.
        if (found) {
            if (gp_vkCmdSetDepthTestEnable)  gp_vkCmdSetDepthTestEnable(cmd, settings.depthTestEnable);
            if (gp_vkCmdSetDepthWriteEnable) gp_vkCmdSetDepthWriteEnable(cmd, settings.depthWriteEnable);
            if (gp_vkCmdSetDepthCompareOp)   gp_vkCmdSetDepthCompareOp(cmd, settings.depthCompareOp);
        }
    }
}

void VKAPI_CALL DetourVkCmdSetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* pVp) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("S");
        loggedOnce = true;
    }

    if (pVp != nullptr && count > 0) {
        std::unique_lock<std::shared_mutex> lock(statesMtx);
        CmdState& state = cmdStates[cmd];
        state.currentViewport = pVp[0];
        state.firstViewport = first;
        state.hasViewport = true;
    }
    if (pOriginalCmdSetViewport) {
        pOriginalCmdSetViewport(cmd, first, count, pVp);
    }
}

void VKAPI_CALL DetourVkCmdDraw(VkCommandBuffer cmd, uint32_t vCount, uint32_t iCount, uint32_t firstV, uint32_t firstI) {
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("3");
        loggedOnce = true;
    }
    pOriginalCmdDraw(cmd, vCount, iCount, firstV, firstI);
}

void VKAPI_CALL DetourVkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t idxCount, uint32_t instCount, uint32_t firstIdx, int32_t vtxOff, uint32_t firstInst) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("4");
        loggedOnce = true;
    }
    
    uint32_t currentStride = 0;
    {
        std::shared_lock<std::shared_mutex> lock(pipeMapMtx);
        auto it = cmdBufferToStride.find(cmd);
        if (it != cmdBufferToStride.end()) currentStride = it->second;
    }

    CmdState localState;
    bool found = false;
    {
        std::shared_lock<std::shared_mutex> lock(statesMtx);
        auto it = cmdStates.find(cmd);
        if (it != cmdStates.end()) {
            localState = it->second;
            found = true;
        }
    }

    // Logic: If stride matches our selection, disable depth test to see through walls
    if (gp_vkCmdSetDepthTestEnable)
        if(currentStride == (uint32_t)countnum|| idxCount/100==countnum || idxCount / 1000 == countnum) {
        gp_vkCmdSetDepthTestEnable(cmd, VK_FALSE); // Disable depth (Wallhack)
        pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
        gp_vkCmdSetDepthTestEnable(cmd, VK_TRUE);  // Restore depth for next object
        return;
    }

    //if (currentStride == (uint32_t)countnum && !gp_vkCmdSetDepthTestEnable) {
      //  vkCmdSetDepthTestEnable(cmd, VK_FALSE); // Disable depth (Wallhack)
        //pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
        //vkCmdSetDepthTestEnable(cmd, VK_TRUE);  // Restore depth for next object
        //return;
    //}
    
    if (found)
        if(currentStride == (uint32_t)countnum || idxCount / 100 == countnum || idxCount / 1000 == countnum) {
        // --- APPLY HACK ---
        const VkViewport originalVp = localState.currentViewport;
        VkViewport hVp = originalVp;

        // Depth range adjustment
        hVp.minDepth = reversedDepth ? 0.0f : 0.9f;
        hVp.maxDepth = reversedDepth ? 0.1f : 1.0f;

        pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &hVp);

        pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

        pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &originalVp);
    }
    
    pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
}

void VKAPI_CALL DetourVkCmdDrawIndirect(VkCommandBuffer cmd, VkBuffer buf, VkDeviceSize off, uint32_t drawCount, uint32_t stride) {
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("5");
        loggedOnce = true;
    }
    pOriginalCmdDrawIndirect(cmd, buf, off, drawCount, stride);
}

//doom the dark ages
void VKAPI_CALL DetourVkCmdDrawIndexedIndirect(
    VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("6");
        loggedOnce = true;
    }

    uint32_t currentStride = 0;
    {
        std::shared_lock<std::shared_mutex> lock(pipeMapMtx);
        auto it = cmdBufferToStride.find(cmd);
        if (it != cmdBufferToStride.end()) currentStride = it->second;
    }

    uint32_t shortCount = (drawCount >> 12) % 100;
    uint32_t shortbuffer = ((uintptr_t)buffer >> 12) % 100;
    uint32_t shortOffset = (offset >> 8) % 100; // or >> 12

    // Identify monsters via Stride or IndexCount
    // If your logger found 'countnum' matches currentStride, hack it!
    bool isMonster = (currentStride == countnum || shortCount == (uint32_t)countnum || shortbuffer == countnum|| shortOffset==countnum);

    // Logic: If stride matches our selection, disable depth test to see through walls
    if (isMonster)
    if (gp_vkCmdSetDepthTestEnable && countnum != -1) {
        gp_vkCmdSetDepthTestEnable(cmd, VK_FALSE); // Disable depth (Wallhack)
        pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
        gp_vkCmdSetDepthTestEnable(cmd, VK_TRUE);  // Restore depth for next object
        return;
    }
    
    return pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
}

void VKAPI_CALL DetourVkCmdDrawIndirectCount(VkCommandBuffer cmd, VkBuffer buf, VkDeviceSize off, VkBuffer cntBuf, VkDeviceSize cntOff, uint32_t maxDraw, uint32_t stride) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("7");
        loggedOnce = true;
    }

    auto pOrig2 = pOriginalCmdDrawIndirectCount;
    if (!pOrig2) return;

    pOriginalCmdDrawIndirectCount(cmd, buf, off, cntBuf, cntOff, maxDraw, stride);
}

void VKAPI_CALL DetourVkCmdDrawIndexedIndirectCount(
    VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset,
    VkBuffer countBuffer, VkDeviceSize countBufferOffset,
    uint32_t maxDrawCount, uint32_t stride)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("8");
        loggedOnce = true;
    }

    auto pOrig = pOriginalCmdDrawIndexedIndirectCount;
    if (!pOrig) return;

    CmdState localState;
    bool found = false;
    {
        std::shared_lock<std::shared_mutex> lock(statesMtx);
        auto it = cmdStates.find(cmd);
        if (it != cmdStates.end()) {
            localState = it->second;
            found = true;
        }
    }

    // Identify the draw call
    uint32_t currentStride = 0;
    {
        std::shared_lock<std::shared_mutex> lock(pipeMapMtx);
        auto it = cmdBufferToStride.find(cmd);
        if (it != cmdBufferToStride.end()) currentStride = it->second;
    }
   
    // Selection logic
    uint32_t shortCount = (maxDrawCount >> 12) % 100;
    uint32_t shortbuffer = ((uintptr_t)buffer >> 12) % 100;
    uint32_t shortOffset = (offset >> 8) % 100; // or >> 12
    uint32_t shortCountBuffer = ((uintptr_t)countBuffer >> 12) % 100;
    uint32_t shortCountOffset = (countBufferOffset >> 8) % 50;

    bool shouldApplyDepthHack = (currentStride == countnum || shortCount == countnum || shortbuffer == countnum ||
        shortOffset == countnum || shortCountBuffer == countnum || shortCountOffset == countnum);

    if (found && shouldApplyDepthHack && localState.hasViewport) {
        
        // --- APPLY HACK ---
        const VkViewport originalVp = localState.currentViewport;
        VkViewport hVp = originalVp;

        // Depth range adjustment
        hVp.minDepth = reversedDepth ? 0.0f : 0.9f;
        hVp.maxDepth = reversedDepth ? 0.1f : 1.0f;

        // Disable depth testing to see through walls
        //gp_vkCmdSetDepthTestEnable(cmd, VK_FALSE);
        //gp_vkCmdSetDepthWriteEnable(cmd, VK_FALSE);
        //gp_vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_ALWAYS);

        pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &hVp);

        // Draw the hacked version
        pOrig(cmd, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);

        // --- RESTORE STATE ---
        // Instead of hardcoding, we just reset to standard behavior or let the next call handle it
        //gp_vkCmdSetDepthTestEnable(cmd, VK_TRUE);
        //gp_vkCmdSetDepthWriteEnable(cmd, VK_TRUE);
        //gp_vkCmdSetDepthCompareOp(cmd, reversedDepth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL);
        pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &originalVp);
    }

    pOrig(cmd, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

// --- Precise Function Matching ---
bool IsVulkanFunc(const char* pName, const char* target) {
    size_t len = strlen(target);
    if (strncmp(pName, target, len) == 0) {
        char nextChar = pName[len];
        // Ensure it's the end of string or a suffix like KHR/EXT (starts with uppercase)
        return (nextChar == '\0' || (nextChar >= 'A' && nextChar <= 'Z'));
    }
    return false;
}

#define INTERCEPT(funcname, detourname, origptr) \
    if (IsVulkanFunc(pName, funcname)) { \
        if (origptr == nullptr) origptr = (decltype(origptr))res; \
        return (PFN_vkVoidFunction)detourname; \
    }


PFN_vkVoidFunction VKAPI_CALL DetourGetDeviceProcAddr(VkDevice device, const char* pName) {
    PFN_vkVoidFunction res = pOriginalGetDeviceProcAddr(device, pName);
    if (!res) return res;

    // Ordered Longest to Shortest to avoid prefix collisions
    INTERCEPT("vkCmdDrawIndexedIndirectCount", DetourVkCmdDrawIndexedIndirectCount, pOriginalCmdDrawIndexedIndirectCount);
    INTERCEPT("vkCmdDrawIndexedIndirect", DetourVkCmdDrawIndexedIndirect, pOriginalCmdDrawIndexedIndirect);
    INTERCEPT("vkCmdDrawIndirectCount", DetourVkCmdDrawIndirectCount, pOriginalCmdDrawIndirectCount);
    INTERCEPT("vkCmdDrawIndirect", DetourVkCmdDrawIndirect, pOriginalCmdDrawIndirect);
    INTERCEPT("vkCmdDrawIndexed", DetourVkCmdDrawIndexed, pOriginalCmdDrawIndexed);
    INTERCEPT("vkCmdSetViewport", DetourVkCmdSetViewport, pOriginalCmdSetViewport);
    INTERCEPT("vkCmdBindPipeline", DetourCmdBindPipeline, pOriginalCmdBindPipeline);
    INTERCEPT("vkCreateGraphicsPipelines", DetourCreateGraphicsPipelines, pOriginalCreateGraphicsPipelines);

    // Capture setters even if we don't detour them

    if (strcmp(pName, "vkCmdSetDepthTestEnable") == 0) {
        gp_vkCmdSetDepthTestEnable = (PFN_vkCmdSetDepthTestEnable)res;
        gSupportsExtendedDynamicState = true;
        Log("Pointer check: TestEnable: %p", gp_vkCmdSetDepthTestEnable);
    }

    if (strcmp(pName, "vkCmdSetDepthWriteEnable") == 0) {
        gp_vkCmdSetDepthWriteEnable = (PFN_vkCmdSetDepthWriteEnable)res;
        gSupportsExtendedDynamicState = true;
    }

    if (strcmp(pName, "vkCmdSetDepthCompareOp") == 0) {
        gp_vkCmdSetDepthCompareOp = (PFN_vkCmdSetDepthCompareOp)res;
        gSupportsExtendedDynamicState = true;
    }

    return res;
}

PFN_vkVoidFunction VKAPI_CALL DetourGetInstanceProcAddr(VkInstance instance, const char* pName) {
    PFN_vkVoidFunction res = pOriginalGetInstanceProcAddr(instance, pName);
    if (!res) return res;

    INTERCEPT("vkGetDeviceProcAddr", DetourGetDeviceProcAddr, pOriginalGetDeviceProcAddr);
    INTERCEPT("vkCreateGraphicsPipelines", DetourCreateGraphicsPipelines, pOriginalCreateGraphicsPipelines);

    return res;
}

// --- Hooking Setup ---
DWORD WINAPI HookThread(LPVOID lpParam) {
    Log("Hook Thread Started");
    HMODULE hVulkan = nullptr;
    while (!(hVulkan = GetModuleHandleA("vulkan-1.dll"))) Sleep(100);

    if (MH_Initialize() != MH_OK) return 1;

    // We MUST hook GetInstanceProcAddr as the entry point
    MH_CreateHook(GetProcAddress(hVulkan, "vkGetInstanceProcAddr"), DetourGetInstanceProcAddr, (LPVOID*)&pOriginalGetInstanceProcAddr);
    MH_CreateHook(GetProcAddress(hVulkan, "vkGetDeviceProcAddr"), DetourGetDeviceProcAddr, (LPVOID*)&pOriginalGetDeviceProcAddr);

    // Standard MinHook fallback for things calling the DLL directly
    MH_CreateHook(GetProcAddress(hVulkan, "vkCreateGraphicsPipelines"), DetourCreateGraphicsPipelines, (LPVOID*)&pOriginalCreateGraphicsPipelines);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdBindPipeline"), DetourCmdBindPipeline, (LPVOID*)&pOriginalCmdBindPipeline);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdSetViewport"), DetourVkCmdSetViewport, (LPVOID*)&pOriginalCmdSetViewport);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdDrawIndexed"), DetourVkCmdDrawIndexed, (LPVOID*)&pOriginalCmdDrawIndexed);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdDrawIndirect"), DetourVkCmdDrawIndirect, (LPVOID*)&pOriginalCmdDrawIndirect);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdDrawIndexedIndirect"), DetourVkCmdDrawIndexedIndirect, (LPVOID*)&pOriginalCmdDrawIndexedIndirect);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdDrawIndirectCount"), DetourVkCmdDrawIndirectCount, (LPVOID*)&pOriginalCmdDrawIndirectCount);
    MH_CreateHook(GetProcAddress(hVulkan, "vkCmdDrawIndexedIndirectCount"), DetourVkCmdDrawIndexedIndirectCount, (LPVOID*)&pOriginalCmdDrawIndexedIndirectCount);

    MH_EnableHook(MH_ALL_HOOKS);

    return 0;
}

DWORD WINAPI InputThread(LPVOID lpParam) {
    while (true) {
        if (GetAsyncKeyState(VK_OEM_COMMA) & 1) { countnum--; }
        if (GetAsyncKeyState(VK_OEM_PERIOD) & 1) { countnum++; }
        if (GetAsyncKeyState(VK_OEM_MINUS) & 1) { countnum = -1; }
        Sleep(20);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID res) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
        CreateThread(NULL, 0, InputThread, NULL, 0, NULL);
    }
    return TRUE;
}

/*
//doom the dark ages:
Hook Thread Started
1
2
S
4
8
*/

/*
//Satisfactory
Hook Thread Started
1
2
S
4
6
5
*/