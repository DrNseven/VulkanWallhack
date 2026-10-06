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


// Needs: <vector> <atomic> <mutex>
// Assumes your existing globals: g_mtx (std::mutex), g_moduleHash, g_pipelineKey,
// g_pipelineStrides (VkPipeline -> uint32_t), g_highlightPipelines (VkPipeline -> VkPipeline),
// combine(), Log(), kVerboseKeyLog, pOriginalCreateGraphicsPipelines.

static constexpr uint32_t kHighlightStride = 40;   // models

// Short lock, lookup only, no calls made while it is held
static bool GetModuleHash(VkShaderModule m, uint64_t& out)
{
    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_moduleHash.find(m);
    if (it == g_moduleHash.end())
        return false;
    out = it->second;
    return true;
}

VKAPI_ATTR VkResult VKAPI_CALL DetourVkCreateGraphicsPipelines(
    VkDevice device,
    VkPipelineCache cache,
    uint32_t count,
    const VkGraphicsPipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    static std::atomic<bool> s_loggedOnce{ false };
    if (!s_loggedOnce.exchange(true))
        Log("DetourVkCreateGraphicsPipelines");

    // 1) Create the original pipelines first
    VkResult r = pOriginalCreateGraphicsPipelines(device, cache, count, pCreateInfos, pAllocator, pPipelines);

    // Don't bail on r != VK_SUCCESS: partial results (e.g. VK_PIPELINE_COMPILE_REQUIRED)
    // still leave valid handles in pPipelines. Each handle is checked below.
    if (!pCreateInfos || !pPipelines || count == 0)
        return r;

    struct Entry
    {
        VkPipeline pipe = VK_NULL_HANDLE;
        uint32_t   stride = 0;
        uint64_t   key = 0;
        VkPipeline highlight = VK_NULL_HANDLE;
    };
    std::vector<Entry> entries;
    entries.reserve(count);

    // NO lock is held while we compute, hash, or create clones.
    for (uint32_t i = 0; i < count; ++i)
    {
        if (pPipelines[i] == VK_NULL_HANDLE)
            continue;

        const VkGraphicsPipelineCreateInfo& ci = pCreateInfos[i];

        Entry e;
        e.pipe = pPipelines[i];

        // ------------------------------------------------------------
        // Dynamic-state flags
        // ------------------------------------------------------------
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

        // ------------------------------------------------------------
        // Pipeline-library awareness (state in a library is ignored
        // unless that library owns it)
        // ------------------------------------------------------------
        bool hasGplInfo = false;
        VkGraphicsPipelineLibraryFlagsEXT gplFlags = 0;
        bool isLinked = false;

        for (auto p = static_cast<const VkBaseInStructure*>(ci.pNext); p; p = p->pNext)
        {
            if (p->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT)
            {
                hasGplInfo = true;
                gplFlags = reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p)->flags;
            }
            else if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR)
            {
                if (reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p)->libraryCount > 0)
                    isLinked = true;
            }
        }

        const bool ownsVertexInput =
            !hasGplInfo || (gplFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
        const bool ownsFragmentOutput =
            !hasGplInfo || (gplFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);

        // pVertexInputState is only meaningful in these cases; otherwise it may be null/junk
        const VkPipelineVertexInputStateCreateInfo* vis =
            (!vinDyn && ownsVertexInput) ? ci.pVertexInputState : nullptr;

        // ------------------------------------------------------------
        // Stride (primary binding = the one attribute 0 reads)
        // ------------------------------------------------------------
        if (!strideDyn && vis &&
            vis->pVertexBindingDescriptions && vis->vertexBindingDescriptionCount > 0)
        {
            uint32_t primary = vis->pVertexBindingDescriptions[0].binding;
            if (vis->pVertexAttributeDescriptions && vis->vertexAttributeDescriptionCount > 0)
                primary = vis->pVertexAttributeDescriptions[0].binding;

            for (uint32_t b = 0; b < vis->vertexBindingDescriptionCount; ++b)
            {
                if (vis->pVertexBindingDescriptions[b].binding == primary)
                {
                    e.stride = vis->pVertexBindingDescriptions[b].stride;
                    break;
                }
            }
        }

        static std::atomic<int> s_strideLogs{ 0 };
        if (s_strideLogs.fetch_add(1) < 2)
            Log("pipe %p stride=%u strideDyn=%d vinDyn=%d flags=0x%x",
                (void*)e.pipe, e.stride, strideDyn ? 1 : 0, vinDyn ? 1 : 0, (unsigned)ci.flags);

        // ------------------------------------------------------------
        // Key
        // ------------------------------------------------------------
        uint64_t key = 0;

        // ---- shader stages ----
        for (uint32_t s = 0; s < ci.stageCount; ++s)
        {
            const VkPipelineShaderStageCreateInfo& st = ci.pStages[s];

            uint64_t h = 0;
            if (GetModuleHash(st.module, h))
            {
                key = combine(key, h);
            }
            else
            {
                key = combine(key, reinterpret_cast<uint64_t>(st.module));
                if (kVerboseKeyLog)
                    Log("CreateGP: module %p not in g_moduleHash (using handle)", (void*)st.module);
            }

            key = combine(key, static_cast<uint64_t>(st.stage));

            if (st.pName)
                for (const char* p = st.pName; *p; ++p)
                    key = combine(key, static_cast<uint8_t>(*p));

            if (st.pSpecializationInfo)
            {
                const VkSpecializationInfo* si = st.pSpecializationInfo;
                if (si->pData && si->dataSize)
                {
                    const uint8_t* d = static_cast<const uint8_t*>(si->pData);
                    for (size_t b = 0; b < si->dataSize; ++b)
                        key = combine(key, d[b]);
                }
                if (si->pMapEntries)
                {
                    for (uint32_t m = 0; m < si->mapEntryCount; ++m)
                    {
                        key = combine(key, si->pMapEntries[m].constantID);
                        key = combine(key, si->pMapEntries[m].offset);
                        key = combine(key, si->pMapEntries[m].size);
                    }
                }
            }
        }

        // ---- vertex input (guarded: vis is null when vertex input is dynamic) ----
        if (vis)
        {
            if (vis->pVertexBindingDescriptions)
            {
                for (uint32_t b = 0; b < vis->vertexBindingDescriptionCount; ++b)
                {
                    key = combine(key, vis->pVertexBindingDescriptions[b].binding);
                    key = combine(key, vis->pVertexBindingDescriptions[b].stride);
                    key = combine(key, vis->pVertexBindingDescriptions[b].inputRate);
                }
            }
            if (vis->pVertexAttributeDescriptions)
            {
                for (uint32_t a = 0; a < vis->vertexAttributeDescriptionCount; ++a)
                {
                    key = combine(key, vis->pVertexAttributeDescriptions[a].location);
                    key = combine(key, vis->pVertexAttributeDescriptions[a].binding);
                    key = combine(key, vis->pVertexAttributeDescriptions[a].format);
                    key = combine(key, vis->pVertexAttributeDescriptions[a].offset);
                }
            }
        }

        // ---- additional pipeline state ----
        if (ci.pInputAssemblyState)
        {
            key = combine(key, ci.pInputAssemblyState->topology);
            key = combine(key, ci.pInputAssemblyState->primitiveRestartEnable ? 1ull : 0ull);
        }
        if (ci.pRasterizationState)
        {
            key = combine(key, ci.pRasterizationState->polygonMode);
            key = combine(key, ci.pRasterizationState->cullMode);
            key = combine(key, ci.pRasterizationState->frontFace);
            key = combine(key, ci.pRasterizationState->depthBiasEnable ? 1ull : 0ull);
        }
        if (ci.pMultisampleState)
            key = combine(key, ci.pMultisampleState->rasterizationSamples);
        if (ci.pDepthStencilState)
        {
            key = combine(key, ci.pDepthStencilState->depthTestEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthWriteEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthCompareOp);
        }
        if (ci.pColorBlendState && ownsFragmentOutput && ci.pColorBlendState->pAttachments)
        {
            key = combine(key, ci.pColorBlendState->attachmentCount);
            for (uint32_t a = 0; a < ci.pColorBlendState->attachmentCount; ++a)
            {
                const auto& att = ci.pColorBlendState->pAttachments[a];
                key = combine(key, att.blendEnable ? 1ull : 0ull);
                key = combine(key, att.colorWriteMask);
            }
        }

        if (key == 0)
            key = combine(key, 0xDEADBEEFCAFEBABEull);

        e.key = key;

        if (kVerboseKeyLog)
            Log("CreateGP: pipeline=%p stages=%u key=%llu", (void*)e.pipe, ci.stageCount, key);

        // ------------------------------------------------------------
        // Highlight clone: only for pipelines we care about.
        // Cloning EVERY pipeline doubles compile cost and causes hitches.
        // ------------------------------------------------------------
        const bool canClone =
            e.stride == kHighlightStride &&
            !isLinked &&
            !(ci.flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) &&
            ownsFragmentOutput &&
            ci.pColorBlendState &&
            ci.pColorBlendState->attachmentCount > 0 &&
            ci.pColorBlendState->pAttachments;

        if (canClone)
        {
            VkGraphicsPipelineCreateInfo highlightCI = ci;

            highlightCI.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
                VK_PIPELINE_CREATE_DERIVATIVE_BIT);
            highlightCI.basePipelineHandle = VK_NULL_HANDLE;
            highlightCI.basePipelineIndex = -1;

            // These locals stay alive until after the create call below
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
            blendState.blendConstants[0] = 1.0f;   // R
            blendState.blendConstants[1] = 0.0f;   // G
            blendState.blendConstants[2] = 0.0f;   // B
            blendState.blendConstants[3] = 1.0f;   // A
            highlightCI.pColorBlendState = &blendState;

            // The game never calls vkCmdSetBlendConstants, so the clone must use the
            // STATIC red above: drop BLEND_CONSTANTS from the dynamic states.
            std::vector<VkDynamicState> dynStates;
            VkPipelineDynamicStateCreateInfo dynInfo{};

            if (ci.pDynamicState && ci.pDynamicState->pDynamicStates)
            {
                for (uint32_t d = 0; d < ci.pDynamicState->dynamicStateCount; ++d)
                    if (ci.pDynamicState->pDynamicStates[d] != VK_DYNAMIC_STATE_BLEND_CONSTANTS)
                        dynStates.push_back(ci.pDynamicState->pDynamicStates[d]);

                dynInfo = *ci.pDynamicState;
                dynInfo.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
                dynInfo.pDynamicStates = dynStates.empty() ? nullptr : dynStates.data();
                highlightCI.pDynamicState = &dynInfo;
            }

            VkPipeline highlightPipe = VK_NULL_HANDLE;
            VkResult hr = pOriginalCreateGraphicsPipelines(
                device, cache, 1, &highlightCI, pAllocator, &highlightPipe);

            if (hr == VK_SUCCESS && highlightPipe != VK_NULL_HANDLE)
            {
                e.highlight = highlightPipe;
                if (kVerboseKeyLog)
                    Log("CreateGP: created highlight pipeline %p for original %p",
                        (void*)highlightPipe, (void*)e.pipe);
            }
            else
            {
                static std::atomic<int> s_failLogs{ 0 };
                if (s_failLogs.fetch_add(1) < 20)
                    Log("highlight create FAILED hr=%d for %p", (int)hr, (void*)e.pipe);
            }
        }

        entries.push_back(e);
    }

    // ------------------------------------------------------------
    // ONE short lock, map writes only
    // ------------------------------------------------------------
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        for (const Entry& e : entries)
        {
            g_pipelineStrides[e.pipe] = e.stride;
            g_pipelineKey[e.pipe] = e.key;

            if (e.highlight != VK_NULL_HANDLE)
                g_highlightPipelines[e.pipe] = e.highlight;
            else
                g_highlightPipelines.erase(e.pipe);   // never keep a stale clone for a reused handle
        }
    }

    return r;
}

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
/*
VkPipeline g_TargetPipe = VK_NULL_HANDLE;   // real handle (optional, for exact match)
uint32_t   g_TargetPipeID = 0;              // short ID (0-99) that you bruteforce

inline uint32_t GetPipeShortID(VkPipeline pipe)
{
    if (pipe == VK_NULL_HANDLE)
        return 0;
    return (reinterpret_cast<uintptr_t>(pipe) >> 12) % 100;
}
*/

void VKAPI_CALL DetourVkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t idxCount, uint32_t instCount, uint32_t firstIdx, int32_t vtxOff, uint32_t firstInst) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexed");
        loggedOnce = true;
    }


    uint64_t   key = 0;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipeline highPipe = VK_NULL_HANDLE;
    uint32_t   dstride = 0;   // dynamic (per command buffer)
    uint32_t   pstride = 0;   // static (per pipeline)
    {
        std::lock_guard<std::mutex> lock(g_mtx);   // the ONLY lock in this function

        auto kit = g_curKey.find(cmd);
        if (kit != g_curKey.end())
            key = kit->second;

        auto pit = g_curPipeline.find(cmd);
        if (pit != g_curPipeline.end())
        {
            pipeline = pit->second;

            auto hit = g_highlightPipelines.find(pipeline);
            if (hit != g_highlightPipelines.end())
                highPipe = hit->second;
        }

        auto sit = g_cmdBufStride.find(cmd);
        if (sit != g_cmdBufStride.end())
            dstride = sit->second;

        // only touch g_pipelineStrides when there is no dynamic stride
        if (dstride == 0 && pipeline != VK_NULL_HANDLE)
        {
            auto pst = g_pipelineStrides.find(pipeline);
            if (pst != g_pipelineStrides.end())
                pstride = pst->second;
        }
    }   // released; nothing below touches g_mtx
    const uint32_t shortKey = static_cast<uint32_t>(key % 100);
    const uint32_t stride = dstride ? dstride : pstride;
  
  

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
    if(stride > 0 && stride == countnum) //40 = valheim
    {
        //return;
        
        if (found && localState.hasViewport) {

            // APPLY HACK
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





    /*
    std::lock_guard<std::mutex> lock(g_mtx);

    auto pipeIt = g_curPipeline.find(cmd);

    //Log("1");

    if (pipeIt == g_curPipeline.end())
    {
        //Log("NO PIPELINE");
        pOriginalCmdDrawIndexed(
            cmd, idxCount, instCount,
            firstIdx, vtxOff, firstInst);
        return;
    }

    //Log("PIPELINE FOUND");

    VkPipeline currentPipe = pipeIt->second;

    //Log("stride=%zu", stride);

    if (stride <= 0 || stride != 40)
    {
        //Log("STRIDE FAILED");
        pOriginalCmdDrawIndexed(
            cmd, idxCount, instCount,
            firstIdx, vtxOff, firstInst);
        return;
    }

    //Log("STRIDE OK");

    auto hlIt = g_highlightPipelines.find(currentPipe);

    if (hlIt == g_highlightPipelines.end())
    {
        //Log("NO HIGHLIGHT PIPELINE");
        pOriginalCmdDrawIndexed(
            cmd, idxCount, instCount,
            firstIdx, vtxOff, firstInst);
        return;
    }

    Log("HIGHLIGHT PIPELINE FOUND");

    VkPipeline highlightPipe = hlIt->second;

    pOriginalCmdBindPipeline(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        highlightPipe
    );

    Log("HIGHLIGHT PIPELINE BOUND");

    const float redColor[4] = {
        1.0f, 0.0f, 0.0f, 1.0f
    };

    vkCmdSetBlendConstants(cmd, redColor);

    Log("BLEND CONSTANT SET");

    pOriginalCmdDrawIndexed(
        cmd,
        idxCount,
        instCount,
        firstIdx,
        vtxOff,
        firstInst
    );

    Log("HIGHLIGHT DRAW");

    pOriginalCmdBindPipeline(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        currentPipe
    );

    */

    /*
    //pipeline
    std::lock_guard<std::mutex> lock(g_mtx);

    auto pipeIt = g_curPipeline.find(cmd);
    if (pipeIt == g_curPipeline.end())
    {
        Log("no pipeline known");
        // no pipeline known → just draw normally
        pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
        return;
    }


    //std::lock_guard<std::mutex> lock(g_mtx);
    //Log("stride == %d", stride);
    // Find the currently bound pipeline for this command buffer
    auto curIt = g_curPipeline.find(cmd);
    {
        Log("1");
        if (curIt != g_curPipeline.end() && stride > 0 && stride == 48)
        {
            Log("2");
            VkPipeline currentPipe = curIt->second;

            // Check if a highlight version exists for the bound pipeline
            auto hlIt = g_highlightPipelines.find(currentPipe);
            if (hlIt != g_highlightPipelines.end())
            {
                Log("3");
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
                pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipe);

                return;
            }
            //else
            //{
                // Reset blend constants back to default white for normal rendering
                //const float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                //vkCmdSetBlendConstants(cmd, defaultColor);
            //}
        }
    }
    */

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



    Sleep(250);   // let the loader finish its first-time init

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = VK_ERROR_INITIALIZATION_FAILED;

    for (int attempt = 0; attempt < 50; ++attempt)   // up to ~10 s
    {
        r = fnCreateInstance(&ici, nullptr, &inst);
        if (r == VK_SUCCESS && inst)
            break;

        Log("dummy: vkCreateInstance attempt %d failed (%d)", attempt, (int)r);
        inst = VK_NULL_HANDLE;
        Sleep(200);
    }

    if (!inst)
    {
        Log("dummy: giving up");

        char path[MAX_PATH]{};
        GetModuleFileNameA(hVk, path, MAX_PATH);
        Log("dummy: vulkan-1.dll = %s", path);

        auto fnVer = (PFN_vkEnumerateInstanceVersion)GetProcAddress(hVk, "vkEnumerateInstanceVersion");
        uint32_t ver = 0;
        if (fnVer && fnVer(&ver) == VK_SUCCESS)
            Log("dummy: loader instance version %u.%u", VK_VERSION_MAJOR(ver), VK_VERSION_MINOR(ver));

        return false;
    }

    /*
    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = fnCreateInstance(&ici, nullptr, &inst);
    if (r != VK_SUCCESS || !inst)
    {
        Log("dummy: vkCreateInstance failed (%d)", (int)r);
        return false;
    }
    */

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