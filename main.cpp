// Vulkan Hook (Valheim Hack)
#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <fstream>
#include <algorithm>
#include <atomic>
#include "vulkan/vulkan.h"
#include "minhook/include/MinHook.h"

#pragma comment(lib, "vulkan/vulkan-1.lib")

//===================================================================================================//

// --- Globals ---
int countnum = -1;
bool reversedDepth = false;

//Log
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

#include <set> 
static void LogRequest(const char* which, const char* name)
{
    if (!which || !name) return;

    // FNV-1a hash of "which:name"
    uint32_t h = 2166136261u;
    for (const char* p = which; *p; ++p) h = (h ^ (uint8_t)*p) * 16777619u;
    h = (h ^ ':') * 16777619u;
    for (const char* p = name; *p; ++p)  h = (h ^ (uint8_t)*p) * 16777619u;

    static std::atomic<uint32_t> seen[512];
    static std::atomic<uint32_t> count{ 0 };

    uint32_t n = count.load();
    if (n > 512) n = 512;
    for (uint32_t i = 0; i < n; ++i)
        if (seen[i].load() == h) return;            // already logged

    uint32_t idx = count.fetch_add(1);
    if (idx >= 512) return;
    seen[idx].store(h);

    Log("%s requested: %s", which, name);           // log only names we haven't seen
}

//Dynamic colors option (not for valheim, but for deadlock)
PFN_vkCmdSetColorBlendEnableEXT pfnCmdSetColorBlendEnableEXT = nullptr;
PFN_vkCmdSetColorBlendEquationEXT pfnCmdSetColorBlendEquationEXT = nullptr;


// Function Pointers
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
typedef void (VKAPI_PTR* PFN_vkCmdBindDescriptorSets)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
typedef VkResult(VKAPI_PTR* PFN_vkCreateShaderModule)(VkDevice device,const VkShaderModuleCreateInfo* pCreateInfo,const VkAllocationCallbacks* pAllocator,VkShaderModule* pModule);

// Typedefs for the new hooks
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
PFN_vkCmdBindDescriptorSets pOriginalCmdBindDescriptorSets = nullptr;
PFN_vkCreateShaderModule pOriginalCreateShaderModule = nullptr;

// Dynamic State Setters 
PFN_vkCmdSetDepthTestEnable  gp_vkCmdSetDepthTestEnable = nullptr;
PFN_vkCmdSetDepthWriteEnable gp_vkCmdSetDepthWriteEnable = nullptr;
PFN_vkCmdSetDepthCompareOp   gp_vkCmdSetDepthCompareOp = nullptr;

// Viewport, Command Buffer State
struct CmdState {
    VkViewport currentViewport;
    uint32_t firstViewport;
    bool hasViewport = false;
};
std::unordered_map<VkCommandBuffer, CmdState> cmdStates;
std::shared_mutex statesMtx;

//Other
static std::mutex g_mtx;
//std::recursive_mutex g_mtx; // Replace std::mutex with std::recursive_mutex
static std::unordered_map<VkShaderModule, size_t>    g_moduleSize;   // module -> codeSize
static std::unordered_map<VkPipeline, uint64_t>      g_pipelineKey;  // pipeline -> key
static std::unordered_map<VkCommandBuffer, uint64_t> g_curKey;       // cmd -> key of bound graphics pipeline

// Toggle for the diagnostic Log() calls below
static constexpr bool kVerboseKeyLog = false;

// Hash helper (64-bit mix)
static uint64_t combine(uint64_t seed, uint64_t v) {
    v *= 0xbf58476d1ce4e5b9ULL;
    v ^= v >> 30;
    v *= 0x94d049bb133111ebULL;
    v ^= v >> 27;
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

// map to store a content hash
static std::unordered_map<VkShaderModule, uint64_t> g_moduleHash;  // module → content hash



// Stride
// Global / singleton / thread-local map (use a lock if multiple threads record)
std::unordered_map<VkCommandBuffer, uint32_t> g_cmdBufStride;          // simple
// or better:
struct VertexInputState {
    uint32_t bindingCount = 0;
    std::vector<VkVertexInputBindingDescription2EXT> bindings;
    uint32_t attributeCount = 0;
    std::vector<VkVertexInputAttributeDescription2EXT> attributes;
};
std::unordered_map<VkCommandBuffer, VertexInputState> g_cmdBufVertexInput;

//===================================================================================================//

VKAPI_ATTR VkResult VKAPI_CALL DetourVkCreateShaderModule(
    VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkShaderModule* pModule)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("CreateShaderModule");
        loggedOnce = true;
    }

    /*
    VkResult r = pOriginalCreateShaderModule(device, pCreateInfo, pAllocator, pModule);
    if (r == VK_SUCCESS && pModule && *pModule != VK_NULL_HANDLE && pCreateInfo) {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_moduleSize[*pModule] = pCreateInfo->codeSize;
        if (kVerboseKeyLog)
            Log("CreateShaderModule: module=%p size=%zu", (void*)*pModule, pCreateInfo->codeSize);
    }
    return r;
    */

    VkResult r = pOriginalCreateShaderModule(device, pCreateInfo, pAllocator, pModule);
    if (r == VK_SUCCESS && pModule && *pModule != VK_NULL_HANDLE && pCreateInfo && pCreateInfo->pCode) {
        uint64_t h = 0;
        const uint32_t* code = pCreateInfo->pCode;
        const size_t wordCount = pCreateInfo->codeSize / 4;

        for (size_t i = 0; i < wordCount; ++i)
            h = combine(h, code[i]);

        // also mix the size so two different-length shaders that somehow produce the same hash still differ
        h = combine(h, pCreateInfo->codeSize);

        std::lock_guard<std::mutex> lock(g_mtx);
        g_moduleHash[*pModule] = h;

        if (kVerboseKeyLog)
            Log("CreateShaderModule: module=%p size=%zu hash=%llu",
                (void*)*pModule, pCreateInfo->codeSize, h);
    }
    return r;
}

//===================================================================================================//

// Add these globals near your other maps
std::unordered_map<VkPipeline, VkPipeline> g_highlightPipelines;   // original → colored version
std::unordered_map<VkCommandBuffer, VkPipeline> g_curPipeline;   // currently bound graphics pipeline

// Correct Map Declaration (Nested Map)
std::unordered_map<VkPipeline, uint32_t> g_pipelineStrides; // pipeline -> binding 0 stride
std::unordered_map<VkCommandBuffer, std::unordered_map<uint32_t, uint32_t>> g_cmdBufStrides;

VKAPI_ATTR VkResult VKAPI_CALL DetourVkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache cache,
    uint32_t count,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCreateGraphicsPipelines");
        loggedOnce = true;
    }

    // 1) Create the original pipelines first
    VkResult r = pOriginalCreateGraphicsPipelines(device, cache, count, pCreateInfos, pAllocator, pPipelines);
    if (r != VK_SUCCESS || !pCreateInfos || !pPipelines)
        return r;

    std::lock_guard<std::mutex> lock(g_mtx);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (pPipelines[i] == VK_NULL_HANDLE)
            continue;

        uint64_t key = 0;
        const VkGraphicsPipelineCreateInfo& ci = pCreateInfos[i];
       
        // 1) Read the dynamic-state flags first
        bool strideDyn = false;
        bool vinDyn = false;

        if (ci.pDynamicState && ci.pDynamicState->pDynamicStates)
        {
            for (uint32_t d = 0; d < ci.pDynamicState->dynamicStateCount; ++d)
            {
                const VkDynamicState s = ci.pDynamicState->pDynamicStates[d];
                if (s == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE) strideDyn = true;
                if (s == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT)            vinDyn = true;
            }
        }
        static bool loggedOnce = false;
        if (!loggedOnce) {
            Log("stride Dynnamic=%d vin Dynnamic=%d", strideDyn, vinDyn);
            loggedOnce = true;
        }

        // 2) Only read the static vertex input when it is actually meaningful
        uint32_t stride = 0;
        const VkPipelineVertexInputStateCreateInfo* vis = ci.pVertexInputState;

        if (!vinDyn && !strideDyn && vis &&
            vis->pVertexBindingDescriptions && vis->vertexBindingDescriptionCount > 0)
        {
            uint32_t primary = vis->pVertexBindingDescriptions[0].binding;
            if (vis->pVertexAttributeDescriptions && vis->vertexAttributeDescriptionCount > 0)
                primary = vis->pVertexAttributeDescriptions[0].binding;

            for (uint32_t b = 0; b < vis->vertexBindingDescriptionCount; ++b)
            {
                if (vis->pVertexBindingDescriptions[b].binding == primary)
                {
                    stride = vis->pVertexBindingDescriptions[b].stride;
                    break;
                }
            }
        }

        //if (pPipelines[i] != VK_NULL_HANDLE)
        //{
            //std::lock_guard<std::mutex> lock(g_mtx);   // keep the lock to the map write only
            //g_pipelineStrides[pPipelines[i]] = stride;
        //}

        //static std::atomic<int> logged{ 0 };
        //if (logged.fetch_add(1) < 5)
            //Log("pipe stride=%u", stride);



        // ---- shader stages ----
        for (uint32_t s = 0; s < ci.stageCount; ++s) {
            const VkPipelineShaderStageCreateInfo& st = ci.pStages[s];
            auto it = g_moduleHash.find(st.module);
            if (it != g_moduleHash.end()) {
                key = combine(key, it->second);
            }
            else {
                key = combine(key, reinterpret_cast<uint64_t>(st.module));
                if (kVerboseKeyLog)
                    Log("CreateGP: module %p not in g_moduleHash (using handle)", (void*)st.module);
            }
            key = combine(key, static_cast<uint64_t>(st.stage));
            if (st.pName) {
                for (const char* p = st.pName; *p; ++p)
                    key = combine(key, static_cast<uint8_t>(*p));
            }
            if (st.pSpecializationInfo) {
                const VkSpecializationInfo* si = st.pSpecializationInfo;
                if (si->pData && si->dataSize) {
                    const uint8_t* d = static_cast<const uint8_t*>(si->pData);
                    for (size_t b = 0; b < si->dataSize; ++b)
                        key = combine(key, d[b]);
                }
                if (si->pMapEntries) {
                    for (uint32_t m = 0; m < si->mapEntryCount; ++m) {
                        key = combine(key, si->pMapEntries[m].constantID);
                        key = combine(key, si->pMapEntries[m].offset);
                        key = combine(key, si->pMapEntries[m].size);
                    }
                }
            }
        }

        // ---- vertex input ----
        if (ci.pVertexInputState) {
            const VkPipelineVertexInputStateCreateInfo* vi = ci.pVertexInputState;
            for (uint32_t b = 0; b < vi->vertexBindingDescriptionCount; ++b) {
                key = combine(key, vi->pVertexBindingDescriptions[b].binding);
                key = combine(key, vi->pVertexBindingDescriptions[b].stride);
                key = combine(key, vi->pVertexBindingDescriptions[b].inputRate);
            }
            for (uint32_t a = 0; a < vi->vertexAttributeDescriptionCount; ++a) {
                key = combine(key, vi->pVertexAttributeDescriptions[a].location);
                key = combine(key, vi->pVertexAttributeDescriptions[a].binding);
                key = combine(key, vi->pVertexAttributeDescriptions[a].format);
                key = combine(key, vi->pVertexAttributeDescriptions[a].offset);
            }
        }

        // ---- additional pipeline state ----
        if (ci.pInputAssemblyState) {
            key = combine(key, ci.pInputAssemblyState->topology);
            key = combine(key, ci.pInputAssemblyState->primitiveRestartEnable ? 1ull : 0ull);
        }
        if (ci.pRasterizationState) {
            key = combine(key, ci.pRasterizationState->polygonMode);
            key = combine(key, ci.pRasterizationState->cullMode);
            key = combine(key, ci.pRasterizationState->frontFace);
            key = combine(key, ci.pRasterizationState->depthBiasEnable ? 1ull : 0ull);
        }
        if (ci.pMultisampleState) {
            key = combine(key, ci.pMultisampleState->rasterizationSamples);
        }
        if (ci.pDepthStencilState) {
            key = combine(key, ci.pDepthStencilState->depthTestEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthWriteEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthCompareOp);
        }
        if (ci.pColorBlendState) {
            key = combine(key, ci.pColorBlendState->attachmentCount);
            for (uint32_t a = 0; a < ci.pColorBlendState->attachmentCount; ++a) {
                const auto& att = ci.pColorBlendState->pAttachments[a];
                key = combine(key, att.blendEnable ? 1ull : 0ull);
                key = combine(key, att.colorWriteMask);
            }
        }

        if (key == 0)
            key = combine(key, 0xDEADBEEFCAFEBABEull);

        g_pipelineKey[pPipelines[i]] = key;

        if (kVerboseKeyLog)
            Log("CreateGP: pipeline=%p stages=%u key=%llu", (void*)pPipelines[i], ci.stageCount, key);

        // ============================================================
        // Solid-Color (Highlight) Pipeline Creation 
        // ============================================================
        if (ci.pColorBlendState && ci.pColorBlendState->attachmentCount > 0)
        {
            VkGraphicsPipelineCreateInfo highlightCI = ci;

            // Strip derivative and completion requirement flags
            highlightCI.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
                VK_PIPELINE_CREATE_DERIVATIVE_BIT);
            highlightCI.basePipelineHandle = VK_NULL_HANDLE;
            highlightCI.basePipelineIndex = -1;

            // Safely copy attachments to prevent stack lifetime mutation
            std::vector<VkPipelineColorBlendAttachmentState> attachments(
                ci.pColorBlendState->pAttachments,
                ci.pColorBlendState->pAttachments + ci.pColorBlendState->attachmentCount);

            for (auto& att : attachments)
            {
                att.blendEnable = VK_TRUE;
                att.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
                att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                att.colorBlendOp = VK_BLEND_OP_ADD;
                att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                att.alphaBlendOp = VK_BLEND_OP_ADD;
            }

            VkPipelineColorBlendStateCreateInfo blendState = *ci.pColorBlendState;
            blendState.pAttachments = attachments.data();
            blendState.blendConstants[0] = 1.0f; // R (Red Highlight)
            blendState.blendConstants[1] = 0.0f; // G
            blendState.blendConstants[2] = 0.0f; // B
            blendState.blendConstants[3] = 1.0f; // A
            highlightCI.pColorBlendState = &blendState;

            // Handle Dynamic States correctly without unsetting mandatory driver structures
            std::vector<VkDynamicState> dynStates;
            VkPipelineDynamicStateCreateInfo dynInfo{};

            if (ci.pDynamicState && ci.pDynamicState->pDynamicStates)
            {
                dynStates.assign(ci.pDynamicState->pDynamicStates,
                    ci.pDynamicState->pDynamicStates + ci.pDynamicState->dynamicStateCount);

                // Preserve existing dynamic states rather than stripping VK_DYNAMIC_STATE_BLEND_CONSTANTS
                dynInfo = *ci.pDynamicState;
                dynInfo.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
                dynInfo.pDynamicStates = dynStates.data();
                highlightCI.pDynamicState = &dynInfo;
            }

            VkPipeline highlightPipe = VK_NULL_HANDLE;
            VkResult hr = pOriginalCreateGraphicsPipelines(
                device, cache, 1, &highlightCI, pAllocator, &highlightPipe);

            if (hr == VK_SUCCESS && highlightPipe != VK_NULL_HANDLE)
            {
                g_highlightPipelines[pPipelines[i]] = highlightPipe;

                if (kVerboseKeyLog)
                    Log("CreateGP: Created highlight pipeline %p for original %p",
                        (void*)highlightPipe, (void*)pPipelines[i]);
            }
        }
    }

    return r;
}

/*
VKAPI_ATTR VkResult VKAPI_CALL DetourVkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache cache,
    uint32_t count,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCreateGraphicsPipelines");
        loggedOnce = true;
    }

    VkResult r = pOriginalCreateGraphicsPipelines(device, cache, count, pCreateInfos, pAllocator, pPipelines);
    if (r != VK_SUCCESS || !pCreateInfos || !pPipelines)
        return r;

    std::lock_guard<std::mutex> lock(g_mtx);

    for (uint32_t i = 0; i < count; ++i) {
        if (pPipelines[i] == VK_NULL_HANDLE)
            continue;

        uint64_t key = 0;
        const VkGraphicsPipelineCreateInfo& ci = pCreateInfos[i];

        // ---- shader stages ----
        for (uint32_t s = 0; s < ci.stageCount; ++s) {
            const VkPipelineShaderStageCreateInfo& st = ci.pStages[s];

            // 1) content hash of the SPIR-V (fallback: module handle)
            auto it = g_moduleHash.find(st.module);
            if (it != g_moduleHash.end()) {
                key = combine(key, it->second);
            }
            else {
                key = combine(key, reinterpret_cast<uint64_t>(st.module));
                if (kVerboseKeyLog)
                    Log("CreateGP: module %p not in g_moduleHash (using handle)", (void*)st.module);
            }

            // 2) stage flag
            key = combine(key, static_cast<uint64_t>(st.stage));

            // 3) entry-point name
            if (st.pName) {
                for (const char* p = st.pName; *p; ++p)
                    key = combine(key, static_cast<uint8_t>(*p));
            }

            // 4) specialization constants
            if (st.pSpecializationInfo) {
                const VkSpecializationInfo* si = st.pSpecializationInfo;
                if (si->pData && si->dataSize) {
                    const uint8_t* d = static_cast<const uint8_t*>(si->pData);
                    for (size_t b = 0; b < si->dataSize; ++b)
                        key = combine(key, d[b]);
                }
                if (si->pMapEntries) {
                    for (uint32_t m = 0; m < si->mapEntryCount; ++m) {
                        key = combine(key, si->pMapEntries[m].constantID);
                        key = combine(key, si->pMapEntries[m].offset);
                        key = combine(key, si->pMapEntries[m].size);
                    }
                }
            }
        }

        // ---- vertex input ----
        if (ci.pVertexInputState) {
            const VkPipelineVertexInputStateCreateInfo* vi = ci.pVertexInputState;
            for (uint32_t b = 0; b < vi->vertexBindingDescriptionCount; ++b) {
                key = combine(key, vi->pVertexBindingDescriptions[b].binding);
                key = combine(key, vi->pVertexBindingDescriptions[b].stride);
                key = combine(key, vi->pVertexBindingDescriptions[b].inputRate);
            }
            for (uint32_t a = 0; a < vi->vertexAttributeDescriptionCount; ++a) {
                key = combine(key, vi->pVertexAttributeDescriptions[a].location);
                key = combine(key, vi->pVertexAttributeDescriptions[a].binding);
                key = combine(key, vi->pVertexAttributeDescriptions[a].format);
                key = combine(key, vi->pVertexAttributeDescriptions[a].offset);
            }
        }

        // ---- additional pipeline state for better uniqueness ----
        if (ci.pInputAssemblyState) {
            key = combine(key, ci.pInputAssemblyState->topology);
            key = combine(key, ci.pInputAssemblyState->primitiveRestartEnable ? 1ull : 0ull);
        }
        if (ci.pRasterizationState) {
            key = combine(key, ci.pRasterizationState->polygonMode);
            key = combine(key, ci.pRasterizationState->cullMode);
            key = combine(key, ci.pRasterizationState->frontFace);
            key = combine(key, ci.pRasterizationState->depthBiasEnable ? 1ull : 0ull);
        }
        if (ci.pMultisampleState) {
            key = combine(key, ci.pMultisampleState->rasterizationSamples);
        }
        if (ci.pDepthStencilState) {
            key = combine(key, ci.pDepthStencilState->depthTestEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthWriteEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthCompareOp);
        }
        if (ci.pColorBlendState) {
            key = combine(key, ci.pColorBlendState->attachmentCount);
            for (uint32_t a = 0; a < ci.pColorBlendState->attachmentCount; ++a) {
                const auto& att = ci.pColorBlendState->pAttachments[a];
                key = combine(key, att.blendEnable ? 1ull : 0ull);
                key = combine(key, att.colorWriteMask);
            }
        }

        // Guarantee non-zero key
        if (key == 0)
            key = combine(key, 0xDEADBEEFCAFEBABEull);

        g_pipelineKey[pPipelines[i]] = key;

        if (kVerboseKeyLog)
            Log("CreateGP: pipeline=%p stages=%u key=%llu", (void*)pPipelines[i], ci.stageCount, key);
    }
    return r;
}
*/
//===================================================================================================//

void VKAPI_CALL DetourVkCmdBindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipeline pipeline) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdBindPipeline");
        loggedOnce = true;
    }

    pOriginalCmdBindPipeline(cmd, bindPoint, pipeline);

    if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS && pipeline != VK_NULL_HANDLE)
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        //std::lock_guard<std::recursive_mutex> lock(g_mtx);

        // Store both the key AND the actual pipeline handle
        auto it = g_pipelineKey.find(pipeline);
        uint64_t key = (it != g_pipelineKey.end()) ? it->second : 0;

        g_curKey[cmd] = key;
        g_curPipeline[cmd] = pipeline;          // we need later

        if (kVerboseKeyLog)
            Log("Bind: cmd=%p pipeline=%p found=%d key=%llu",
                (void*)cmd, (void*)pipeline, it != g_pipelineKey.end(), key);
    }
}

//===================================================================================================//

void VKAPI_CALL DetourVkCmdSetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* pVp) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdSetViewport");
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

//===================================================================================================//

// Typedef
typedef void (VKAPI_PTR* PFN_vkCmdSetVertexInputEXT)(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions,
    uint32_t                                    vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);

PFN_vkCmdSetVertexInputEXT pOriginalCmdSetVertexInputEXT = nullptr;

// Detour
void VKAPI_CALL DetourVkCmdSetVertexInputEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions,
    uint32_t                                    vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdSetVertexInputEXT");
        loggedOnce = true;
    }
    
    // capture
    if (pVertexBindingDescriptions && vertexBindingDescriptionCount > 0)
    {
        // simplest: just keep binding 0 stride
        g_cmdBufStride[commandBuffer] = pVertexBindingDescriptions[0].stride;

        // richer version 
        VertexInputState& state = g_cmdBufVertexInput[commandBuffer];
        state.bindingCount = vertexBindingDescriptionCount;
        state.bindings.assign(pVertexBindingDescriptions,
            pVertexBindingDescriptions + vertexBindingDescriptionCount);
        state.attributeCount = vertexAttributeDescriptionCount;
        state.attributes.assign(pVertexAttributeDescriptions,
            pVertexAttributeDescriptions + vertexAttributeDescriptionCount);
    }

    pOriginalCmdSetVertexInputEXT(
        commandBuffer,
        vertexBindingDescriptionCount,
        pVertexBindingDescriptions,
        vertexAttributeDescriptionCount,
        pVertexAttributeDescriptions);
}

//===================================================================================================//

VkPipeline g_TargetPipe = VK_NULL_HANDLE;   // real handle (optional, for exact match)
uint32_t   g_TargetPipeID = 0;              // short ID (0-99) that you bruteforce

inline uint32_t GetPipeShortID(VkPipeline pipe)
{
    if (pipe == VK_NULL_HANDLE)
        return 0;
    return (reinterpret_cast<uintptr_t>(pipe) >> 12) % 100;
}


void VKAPI_CALL DetourVkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t idxCount, uint32_t instCount, uint32_t firstIdx, int32_t vtxOff, uint32_t firstInst) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexed");
        loggedOnce = true;
    }

    //key
    uint64_t key = 0;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto it = g_curKey.find(cmd);
        if (it != g_curKey.end())
            key = it->second;
    }
    uint32_t shortKey = static_cast<uint32_t>(key % 100);   // 0 … 99

    //if (kVerboseKeyLog)
    //if (idxCount == 52647)
        //Log("DrawIndexed: cmd==%p && key==%llu && drawCount=%u && stride==%d", (void*)cmd, key, drawCount);

    //stride
    /*
    uint32_t stride = 0;
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        auto curPipeIt = g_curPipeline.find(cmd);
        if (curPipeIt != g_curPipeline.end() && curPipeIt->second != VK_NULL_HANDLE)
        {
            auto pipeStrideIt = g_pipelineStrides.find(curPipeIt->second);
            if (pipeStrideIt != g_pipelineStrides.end())
                stride = pipeStrideIt->second;
        }
    }
    */
    //if(stride > 0)
    //Log("stride == %d", stride);
    
    //uint32_t stride = 0;
    //{
        // 2) Fallback: Fetch from dynamic command buffer state if pipeline lookup was empty
        //std::lock_guard<std::mutex> lock(g_mtx);
        //auto it = g_cmdBufStride.find(cmd);
        //if (it != g_cmdBufStride.end())
            //stride = it->second;
    //}

    /*
    // Look up the vertex-input state that was previously recorded
    // for this command buffer via DetourVkCmdSetVertexInputEXT
    auto it = g_cmdBufVertexInput.find(cmd);
    if (it != g_cmdBufVertexInput.end())
    {
        const VertexInputState& state = it->second;

        // ------------------------------------------------------------------
        // Example usage – pick whatever you need for model recognition
        // ------------------------------------------------------------------

        // 1. Stride of binding 0 (most common case)
        uint32_t stride0 = 0;
        if (state.bindingCount > 0)
            stride0 = state.bindings[0].stride;

        // 2. All binding strides
        for (uint32_t i = 0; i < state.bindingCount; ++i)
        {
            uint32_t stride = state.bindings[i].stride;
            uint32_t binding = state.bindings[i].binding;
            // … use them …
        }

        // 3. Attribute descriptions (location / format / offset)
        for (uint32_t i = 0; i < state.attributeCount; ++i)
        {
            const auto& attr = state.attributes[i];
            // attr.location, attr.binding, attr.format, attr.offset
            // These are usually the best fingerprint for a mesh
        }

        // 4. Simple hash of the whole state (good for recognition)
        //    (you would implement your own hash function)
        // size_t hash = HashVertexInputState(state);
        // if (hash == knownPlayerHash) { … }
    }
    */


    //viewport
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


    //if (shortKey == countnum)
    if(key == 49)//40 = valheim
    {
        if (found && localState.hasViewport) {

            // --- APPLY HACK ---
            const VkViewport originalVp = localState.currentViewport;
            VkViewport hVp = originalVp;

            // Depth range adjustment
            hVp.minDepth = reversedDepth ? 0.0f : 0.9f;
            hVp.maxDepth = reversedDepth ? 0.1f : 1.0f;


            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &hVp);

            // Draw the hacked version
            pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &originalVp);
        }
    }


    //pipeline
    std::lock_guard<std::mutex> lock(g_mtx);

    auto pipeIt = g_curPipeline.find(cmd);
    if (pipeIt == g_curPipeline.end())
    {
        // no pipeline known → just draw normally
        pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
        return;
    }


    //std::lock_guard<std::mutex> lock(g_mtx);
    //Log("stride == %d", stride);
    // Find the currently bound pipeline for this command buffer
    auto curIt = g_curPipeline.find(cmd);
    if(key == 49)
    if (curIt != g_curPipeline.end())
    {
        VkPipeline currentPipe = curIt->second;

        // Check if a highlight version exists for the bound pipeline
        auto hlIt = g_highlightPipelines.find(currentPipe);
        if (hlIt != g_highlightPipelines.end())
        {
            VkPipeline highlightPipe = hlIt->second;

            // 1) Bind the solid-color highlight pipeline
            pOriginalCmdBindPipeline(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                highlightPipe
            );

            // 2) Set your custom RGBA color (e.g., Red: 1.0, 0.0, 0.0, 1.0)
            const float redColor[4] = { 255.0f, 0.0f, 0.0f, 1.0f };
            vkCmdSetBlendConstants(cmd, redColor);

            // 3) Draw with the highlight pipeline
            pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

            // 4) Restore original pipeline state back to the command buffer
            pOriginalCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,currentPipe);

            return;
        }
        //else
        //{
            // Reset blend constants back to default white for normal rendering
            //const float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            //vkCmdSetBlendConstants(cmd, defaultColor);
        //}
    }

    //VkPipeline originalPipeline = pipeIt->second;          // ← this is “the original pipeline handle”
    /*
    VkPipeline originalPipeline = VK_NULL_HANDLE;
    VkPipeline highPipe = VK_NULL_HANDLE;
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        auto pit = g_curPipeline.find(cmd);
        if (pit != g_curPipeline.end())
        {
            originalPipeline = pit->second;
            auto hit = g_highlightPipelines.find(originalPipeline);
            if (hit != g_highlightPipelines.end())
                highPipe = hit->second;
        }
    }   // g_mtx released here; nothing below touches it

    const bool swap = (stride == 40) && highPipe != VK_NULL_HANDLE;

    if (swap)
        pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, highPipe);

    pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

    if (swap)
        pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, originalPipeline);
    */

    /*
    auto highIt = g_highlightPipelines.find(originalPipeline);
    //if(shortKey == countnum)
    //if(stride == 40)
    if (shortKey == countnum && highIt != g_highlightPipelines.end())
    {
        // temporarily bind the highlight version
        pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, highIt->second);

        const float blendConstants[4] = { 1.0f, 0.0f, 0.0f, 1.0f }; // red
        vkCmdSetBlendConstants(cmd, blendConstants);
    }
    //else
    //{
      //  float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        //vkCmdSetBlendConstants(cmd, defaultColor);
    //}
    */

    /*
    auto it = pipelineData.find(currentPipe);
    if (it != pipelineData.end())
    {
        PipelineSettings& s = it->second;

        uint32_t currentPipeID = GetPipeShortID(currentPipe);

        if (s.stride == countnum)// && currentBoundSets == 2 && shortvoffset == countnum)
            //if (s.stride == 12 && s.attrCount == 6 && shortPushID == 1148||//push.firstU32 == countnum)//g_TargetPipeID) //13
                //s.stride == 12 && s.attrCount == 6 && shortPushID == 2296)
            //if (s.stride == 12 && s.attrCount == 6 && shortPushID == 1148||
                //s.stride == 12 && s.attrCount == 6 && shortPushID == 0)
                // or the stricter version:
                // if (currentPipeID == g_TargetPipeID && s.stride == 12 && s.attrCount == 6 ...)
        {
            Log("s.stride == %d && s.attrCount == %d && s.stride1 == %d && s.stride2 == %d && s.format0 == %d && s.depthTestEnable == %d && s.offsetSum == %d && s.depthCompareOp == %d && s.dnaHash == %d",
                s.stride, s.attrCount, s.stride1, s.stride2, s.format0, s.depthTestEnable, s.offsetSum, s.depthCompareOp, s.dnaHash);
            //s.stride == 48 && s.stride1 == 4 && s.stride2 == 0 && s.stride3 == 0 && s.stride4 == 0 && s.format0 == 91 && s.depthTestEnable == 1 && s.offsetSum == 232 && 
            //s.depthCompareOp == 3 && s.dnaHash == 50876163
            //Log("s.attrCount == %d && idxCount=%u && shortvoffset == %d && boundVBuffer == %d && bruteforceSetCount == %d && currentLayout == %d && currentBoundSets == %d", 
                //s.attrCount, idxCount, shortvoffset, boundVBuffer, bruteforceSetCount, currentLayout, currentBoundSets);
            //Log("g_TargetPipe == %p && g_TargetPipeID == %d && countnum == %d", g_TargetPipe, g_TargetPipeID, countnum);
            float myCustomColor[4] = { 25.0f, 255.0f, 0.0f, 1.0f };
            vkCmdSetBlendConstants(cmd, myCustomColor);
        }
        else
        {
            float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            vkCmdSetBlendConstants(cmd, defaultColor);
        }
    }
    */

        /*
        //dynamic coloring example (does not work in valheim, only works in very few games like deadlock)
        VkBool32 enable = VK_TRUE;

        //vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);
        pfnCmdSetColorBlendEnableEXT(cmd, 0, 1, &enable);

        VkColorBlendEquationEXT eq{};
        eq.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
        eq.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        eq.colorBlendOp = VK_BLEND_OP_ADD;

        eq.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        eq.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        eq.alphaBlendOp = VK_BLEND_OP_ADD;

        //vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &eq);
        pfnCmdSetColorBlendEquationEXT(cmd, 0, 1, &eq);

        const float blendConstants[4] = {
            1.0f, 0.0f, 0.0f, 1.0f
        };

        vkCmdSetBlendConstants(cmd, blendConstants);
        */
    
    //else
    //{
        //float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        //vkCmdSetBlendConstants(cmd, defaultColor);
    //}

    pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
}

//===================================================================================================//

void VKAPI_CALL DetourVkCmdDrawIndexedIndirect(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexedIndirect");
        loggedOnce = true;
    }
    
    return pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
}

//===================================================================================================//

/*
    pfnCmdSetColorBlendEnableEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEnableEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEnableEXT"));
    pfnCmdSetColorBlendEquationEXT = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(device, "vkCmdSetColorBlendEquationEXT"));
    
    if (!pfnCmdSetColorBlendEnableEXT ||
        !pfnCmdSetColorBlendEquationEXT)
    {
        static bool loggedOnce = false;
        if (!loggedOnce) {
            //this is just the dynamic coloring option that will not work in most games, works in deadlock
            Log("pfnCmdSetColorBlend Extension/Function isn't available. (It's fine)");
            loggedOnce = true;
        }
    }
*/

//===================================================================================================//

static std::vector<void*> g_patchedAddrs;   // only touched from HookThread

static bool HookFn(const char* name, PFN_vkVoidFunction fn, void* detour, void** original)
{
    if (!fn)
    {
        Log("hook %s: driver returned null", name);
        return false;
    }

    for (void* a : g_patchedAddrs)
    {
        if (a == (void*)fn)
        {
            Log("hook %s @%p: already patched, skipping", name, (void*)fn);
            return true;
        }
    }

    MH_STATUS s = MH_CreateHook((void*)fn, detour, original);
    if (s == MH_OK)
        s = MH_EnableHook((void*)fn);

    Log("hook %s @%p -> %s", name, (void*)fn, MH_StatusToString(s));

    if (s == MH_OK)
    {
        g_patchedAddrs.push_back((void*)fn);
        return true;
    }
    return false;
}

static bool HookViaDummyDevice()
{
    HMODULE hVk = GetModuleHandleA("vulkan-1.dll");
    if (!hVk)
    {
        Log("dummy: vulkan-1.dll not loaded");
        return false;
    }

    // Loader functions, resolved from the exports (these are NOT hooked by us,
    // except the two resolvers, which we avoid by using the saved originals).
    auto fnGetInstanceProcAddr = pOriginalGetInstanceProcAddr
        ? pOriginalGetInstanceProcAddr
        : (PFN_vkGetInstanceProcAddr)GetProcAddress(hVk, "vkGetInstanceProcAddr");

    auto fnCreateInstance =
        (PFN_vkCreateInstance)GetProcAddress(hVk, "vkCreateInstance");

    if (!fnGetInstanceProcAddr || !fnCreateInstance)
    {
        Log("dummy: loader entry points missing");
        return false;
    }

    // ---- instance ----
    VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pApplicationName = "dummy";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &app;

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = fnCreateInstance(&ici, nullptr, &inst);
    if (r != VK_SUCCESS || !inst)
    {
        Log("dummy: vkCreateInstance failed (%d)", (int)r);
        return false;
    }

    auto fnEnumPhys = (PFN_vkEnumeratePhysicalDevices)
        fnGetInstanceProcAddr(inst, "vkEnumeratePhysicalDevices");
    auto fnQueueFamilies = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
        fnGetInstanceProcAddr(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
    auto fnEnumDevExt = (PFN_vkEnumerateDeviceExtensionProperties)
        fnGetInstanceProcAddr(inst, "vkEnumerateDeviceExtensionProperties");
    auto fnCreateDevice = (PFN_vkCreateDevice)
        fnGetInstanceProcAddr(inst, "vkCreateDevice");
    auto fnGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)
        fnGetInstanceProcAddr(inst, "vkGetDeviceProcAddr");

    if (!fnEnumPhys || !fnQueueFamilies || !fnEnumDevExt || !fnCreateDevice || !fnGetDeviceProcAddr)
    {
        Log("dummy: instance-level entry points missing");
        return false;   // instance intentionally kept alive
    }

    uint32_t gpuCount = 0;
    fnEnumPhys(inst, &gpuCount, nullptr);
    if (gpuCount == 0)
    {
        Log("dummy: no physical devices");
        return false;
    }

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    fnEnumPhys(inst, &gpuCount, gpus.data());


    // ---- functions to patch ----
    struct Entry { const char* name; void* detour; void** original; };
    const Entry list[] = {
        { "vkCreateGraphicsPipelines", (void*)DetourVkCreateGraphicsPipelines, (void**)&pOriginalCreateGraphicsPipelines },
        { "vkCmdBindPipeline",         (void*)DetourVkCmdBindPipeline,         (void**)&pOriginalCmdBindPipeline },
        { "vkCreateShaderModule",      (void*)DetourVkCreateShaderModule,      (void**)&pOriginalCreateShaderModule },
        { "vkCmdSetVertexInputEXT",    (void*)DetourVkCmdSetVertexInputEXT,    (void**)&pOriginalCmdSetVertexInputEXT },
        { "vkCmdSetViewport",          (void*)DetourVkCmdSetViewport,          (void**)&pOriginalCmdSetViewport },
        { "vkCmdDrawIndexed",          (void*)DetourVkCmdDrawIndexed,          (void**)&pOriginalCmdDrawIndexed },
        { "vkCmdDrawIndexedIndirect",  (void*)DetourVkCmdDrawIndexedIndirect,  (void**)&pOriginalCmdDrawIndexedIndirect },
        
    };

    bool anyPatched = false;

    for (VkPhysicalDevice pd : gpus)
    {
        // Needs VK_EXT_vertex_input_dynamic_state, otherwise skip this GPU
        uint32_t extCount = 0;
        fnEnumDevExt(pd, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        fnEnumDevExt(pd, nullptr, &extCount, exts.data());

        bool hasVin = false;
        for (const auto& e : exts)
        {
            if (!strcmp(e.extensionName, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME))
            {
                hasVin = true;
                break;
            }
        }

        if (!hasVin)
        {
            Log("dummy: GPU lacks %s, skipping", VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME);
            continue;
        }

        // Pick a graphics queue family instead of assuming 0
        uint32_t qCount = 0;
        fnQueueFamilies(pd, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        fnQueueFamilies(pd, &qCount, qProps.data());

        uint32_t family = 0;
        for (uint32_t q = 0; q < qCount; ++q)
        {
            if (qProps[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                family = q;
                break;
            }
        }

        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = family;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;

        VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vinFeat{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT };
        vinFeat.vertexInputDynamicState = VK_TRUE;

        const char* devExts[] = { VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME };

        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.pNext = &vinFeat;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = devExts;

        VkDevice dev = VK_NULL_HANDLE;
        r = fnCreateDevice(pd, &dci, nullptr, &dev);
        if (r != VK_SUCCESS || !dev)
        {
            Log("dummy: vkCreateDevice failed (%d)", (int)r);
            continue;
        }

        for (const Entry& e : list)
        {
            if (HookFn(e.name, fnGetDeviceProcAddr(dev, e.name), e.detour, e.original))
                anyPatched = true;
        }

        // No break
    }

    Log("dummy: done, anyPatched=%d", anyPatched ? 1 : 0);
    return anyPatched;
}

DWORD WINAPI HookThread(LPVOID)
{
    Log("Hook Thread Started");

    HMODULE hVulkan = nullptr;
    while (!(hVulkan = GetModuleHandleA("vulkan-1.dll")))
        Sleep(1);

    if (MH_Initialize() != MH_OK)
        return 1;

    // Optional: keep resolver hooks for games that resolve late?
    //MH_CreateHook(GetProcAddress(hVulkan, "vkGetInstanceProcAddr"), DetourGetInstanceProcAddr, (LPVOID*)&pOriginalGetInstanceProcAddr);
    //MH_CreateHook(GetProcAddress(hVulkan, "vkGetDeviceProcAddr"), DetourGetDeviceProcAddr, (LPVOID*)&pOriginalGetDeviceProcAddr);
    //MH_EnableHook(MH_ALL_HOOKS);

    HookViaDummyDevice();
    return 0;
}

DWORD WINAPI InputThread(LPVOID lpParam) {
    while (true) {
        if (GetAsyncKeyState(VK_OEM_COMMA) & 1) { countnum--; }
        if (GetAsyncKeyState(VK_OEM_PERIOD) & 1) { countnum++; }
        if (GetAsyncKeyState(VK_OEM_MINUS) & 1) { countnum = -1; }
        Sleep(1);
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

extern "C" __declspec(dllexport)
LRESULT CALLBACK NextHook(int code, WPARAM wParam, LPARAM lParam)
{
    return CallNextHookEx(NULL, code, wParam, lParam);
}