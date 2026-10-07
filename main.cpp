// Vulkan Hook/Wallhack
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
//stride value needs to be correctfor coloring to work
static constexpr uint32_t kHighlightStride = 28;   //40 = valheim, 48 = zombie army 4: dead war, 28 = deadlock

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

//===================================================================================================//

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
typedef void (VKAPI_PTR* PFN_vkCmdSetVertexInputEXT)(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions,
    uint32_t                                    vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);

// Typedefs
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
PFN_vkCmdSetVertexInputEXT pOriginalCmdSetVertexInputEXT = nullptr;

//===================================================================================================//

// Toggle for the diagnostic Log() calls below
static constexpr bool kVerboseKeyLog = false;

// Viewport, Command Buffer State
struct CmdState {
    VkViewport currentViewport;
    uint32_t firstViewport;
    bool hasViewport = false;
};
std::unordered_map<VkCommandBuffer, CmdState> cmdStates;
std::shared_mutex statesMtx;

//===================================================================================================//

//Pipeline key
static std::mutex g_mtx;
static std::unordered_map<VkPipeline, uint64_t>      g_pipelineKey;  // pipeline -> key
static std::unordered_map<VkCommandBuffer, uint64_t> g_curKey;       // cmd -> key of bound graphics pipeline
static std::unordered_map<VkShaderModule, uint64_t> g_moduleHash;  // module → content hash

// Hash helper (64-bit mix)
static uint64_t combine(uint64_t seed, uint64_t v) {
    v *= 0xbf58476d1ce4e5b9ULL;
    v ^= v >> 30;
    v *= 0x94d049bb133111ebULL;
    v ^= v >> 27;
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

//===================================================================================================//

// Stride (dynamic version)
std::unordered_map<VkCommandBuffer, uint32_t> g_cmdBufStride;

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

// Maps
std::unordered_map<VkPipeline, VkPipeline> g_highlightPipelines;   // original → colored version
std::unordered_map<VkCommandBuffer, VkPipeline> g_curPipeline;   // currently bound graphics pipeline
std::unordered_map<VkPipeline, uint32_t> g_pipelineStrides; // pipeline -> binding 0 stride

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

//===================================================================================================//

// ============================================================================
// DetourVkCreateGraphicsPipelines  (VK_EXT_graphics_pipeline_library aware)
//
// Assumes you already have: pOriginalCreateGraphicsPipelines, g_mtx,
// g_pipelineStrides, g_pipelineKey, g_highlightPipelines, kHighlightStride,
// kVerboseKeyLog, GetModuleHash(), combine(), Log().
//
// Needs: <vector> <unordered_map> <atomic> <mutex>
// ============================================================================

// ---- add near your other globals ------------------------------------------

struct LibInfo
{
    VkGraphicsPipelineLibraryFlagsEXT parts = 0;   // which GPL parts this pipeline contains
    uint32_t   stride = 0;                         // from the vertex-input part (0 if dynamic)
    bool       strideUnknown = false;              // vertex input / stride is dynamic
    bool       hasFrag = false;                    // contains a fragment shader stage
    uint64_t   key = 0;                            // hash of the state this part owns
    VkPipeline highlightLib = VK_NULL_HANDLE;      // only set for fragment-output parts
};

std::unordered_map<VkPipeline, LibInfo> g_libInfo; // guarded by g_mtx

static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplVI =
VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplPre =
VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplFS =
VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplFO =
VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplAll = kGplVI | kGplPre | kGplFS | kGplFO;

// Owns the memory a highlight create-info points into. Must outlive the create call.
struct HighlightStorage
{
    std::vector<VkPipelineColorBlendAttachmentState> attachments;
    VkPipelineColorBlendStateCreateInfo              blend{};
    std::vector<VkDynamicState>                      dynStates;
    VkPipelineDynamicStateCreateInfo                 dynInfo{};
};

// Clone of ci with constant-color red blend. Only valid when ci owns the
// fragment-output state (pColorBlendState is real).
static VkGraphicsPipelineCreateInfo BuildHighlightCI(const VkGraphicsPipelineCreateInfo& ci,
    HighlightStorage& hs)
{
    VkGraphicsPipelineCreateInfo h = ci;
    h.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
        VK_PIPELINE_CREATE_DERIVATIVE_BIT);
    h.basePipelineHandle = VK_NULL_HANDLE;
    h.basePipelineIndex = -1;

    hs.attachments.assign(ci.pColorBlendState->pAttachments,
        ci.pColorBlendState->pAttachments + ci.pColorBlendState->attachmentCount);
    for (auto& att : hs.attachments)
    {
        att.blendEnable = VK_TRUE;
        att.srcColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
        att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        att.colorBlendOp = VK_BLEND_OP_ADD;
        att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    hs.blend = *ci.pColorBlendState;
    hs.blend.pAttachments = hs.attachments.data();
    hs.blend.blendConstants[0] = 55.0f;   // R (1.0 isn't bright enough / can go black)
    hs.blend.blendConstants[1] = 0.0f;
    hs.blend.blendConstants[2] = 0.0f;
    hs.blend.blendConstants[3] = 1.0f;
    h.pColorBlendState = &hs.blend;

    // The game never calls vkCmdSetBlendConstants -> use the static red above.
    if (ci.pDynamicState && ci.pDynamicState->pDynamicStates)
    {
        for (uint32_t d = 0; d < ci.pDynamicState->dynamicStateCount; ++d)
            if (ci.pDynamicState->pDynamicStates[d] != VK_DYNAMIC_STATE_BLEND_CONSTANTS)
                hs.dynStates.push_back(ci.pDynamicState->pDynamicStates[d]);

        hs.dynInfo = *ci.pDynamicState;
        hs.dynInfo.dynamicStateCount = static_cast<uint32_t>(hs.dynStates.size());
        hs.dynInfo.pDynamicStates = hs.dynStates.empty() ? nullptr : hs.dynStates.data();
        h.pDynamicState = &hs.dynInfo;
    }
    return h;
}

// ---- the detour -------------------------------------------------------------

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

    // Partial results (e.g. VK_PIPELINE_COMPILE_REQUIRED) still leave valid handles.
    if (!pCreateInfos || !pPipelines || count == 0)
        return r;

    struct Entry
    {
        VkPipeline pipe = VK_NULL_HANDLE;
        bool       isLibrary = false;
        uint32_t   stride = 0;
        uint64_t   key = 0;
        VkPipeline highlight = VK_NULL_HANDLE;  // complete (non-library) highlight pipeline
        LibInfo    lib;                         // filled when isLibrary
    };

    std::vector<Entry> entries;
    entries.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (pPipelines[i] == VK_NULL_HANDLE)
            continue;

        const VkGraphicsPipelineCreateInfo& ci = pCreateInfos[i];

        Entry e;
        e.pipe = pPipelines[i];

        const bool isLibrary = (ci.flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) != 0;
        e.isLibrary = isLibrary;

        // ------------------------------------------------------------
        // Dynamic-state flags (of this create info)
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
        // pNext chain: GPL part flags + linked libraries
        // ------------------------------------------------------------
        const VkGraphicsPipelineLibraryCreateInfoEXT* gpl = nullptr;
        const VkPipelineLibraryCreateInfoKHR* li = nullptr;

        for (auto p = static_cast<const VkBaseInStructure*>(ci.pNext); p; p = p->pNext)
        {
            if (p->sType == VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT)
                gpl = reinterpret_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(p);
            else if (p->sType == VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR)
                li = reinterpret_cast<const VkPipelineLibraryCreateInfoKHR*>(p);
        }

        const bool isLinked = li && li->libraryCount > 0 && li->pLibraries;

        // ------------------------------------------------------------
        // Merge info from linked libraries (stride, key, parts, frag stage)
        // and build the list with fragment-output libs swapped for highlight libs.
        // ------------------------------------------------------------
        VkGraphicsPipelineLibraryFlagsEXT libParts = 0;
        uint64_t libKey = 0;
        uint32_t libStride = 0;
        bool     libStrideUnknown = false;
        bool     libHasFrag = false;
        bool     swapped = false;
        std::vector<VkPipeline> hlLibs;

        if (isLinked)
        {
            hlLibs.assign(li->pLibraries, li->pLibraries + li->libraryCount);

            std::lock_guard<std::mutex> lock(g_mtx);
            for (VkPipeline& lib : hlLibs)
            {
                auto it = g_libInfo.find(lib);
                if (it == g_libInfo.end())
                    continue;   // library created before the hook was live

                const LibInfo& L = it->second;
                libParts |= L.parts;
                libKey = combine(libKey, L.key);
                libHasFrag |= L.hasFrag;

                if (L.parts & kGplVI)
                {
                    libStride = L.stride;
                    libStrideUnknown = L.strideUnknown;
                }

                if ((L.parts & kGplFO) && L.highlightLib != VK_NULL_HANDLE)
                {
                    lib = L.highlightLib;
                    swapped = true;
                }
            }
        }

        // Which parts does THIS create info actually own?
        // State for parts it doesn't own is ignored (and may be null/junk).
        VkGraphicsPipelineLibraryFlagsEXT owned;
        if (gpl)            owned = gpl->flags;
        else if (isLinked)  owned = libParts ? (kGplAll & ~libParts) : 0;
        else                owned = kGplAll;   // classic monolithic pipeline

        const bool ownsVI = (owned & kGplVI) != 0;
        const bool ownsPre = (owned & kGplPre) != 0;
        const bool ownsFS = (owned & kGplFS) != 0;
        const bool ownsFO = (owned & kGplFO) != 0;

        // ------------------------------------------------------------
        // Stride (only from the vertex-input part)
        // ------------------------------------------------------------
        const VkPipelineVertexInputStateCreateInfo* vis =
            (ownsVI && !vinDyn) ? ci.pVertexInputState : nullptr;

        uint32_t ownStride = 0;
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
                    ownStride = vis->pVertexBindingDescriptions[b].stride;
                    break;
                }
            }
        }

        const uint32_t stride = ownsVI ? ownStride : libStride;
        const bool strideUnknown = ownsVI ? (vinDyn || strideDyn) : libStrideUnknown;
        e.stride = stride;

        // ------------------------------------------------------------
        // Key (own state only, then combined with the libraries' keys)
        // ------------------------------------------------------------
        uint64_t key = 0;
        bool ownHasFrag = false;

        if (ci.pStages && (ownsPre || ownsFS))
        {
            for (uint32_t s = 0; s < ci.stageCount; ++s)
            {
                const VkPipelineShaderStageCreateInfo& st = ci.pStages[s];

                if (st.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
                    ownHasFrag = true;

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
        }

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

        if (ownsVI && ci.pInputAssemblyState)
        {
            key = combine(key, ci.pInputAssemblyState->topology);
            key = combine(key, ci.pInputAssemblyState->primitiveRestartEnable ? 1ull : 0ull);
        }

        if (ownsPre && ci.pRasterizationState)
        {
            key = combine(key, ci.pRasterizationState->polygonMode);
            key = combine(key, ci.pRasterizationState->cullMode);
            key = combine(key, ci.pRasterizationState->frontFace);
            key = combine(key, ci.pRasterizationState->depthBiasEnable ? 1ull : 0ull);
        }

        if ((ownsFS || ownsFO) && ci.pMultisampleState)
            key = combine(key, ci.pMultisampleState->rasterizationSamples);

        if (ownsFS && ci.pDepthStencilState)
        {
            key = combine(key, ci.pDepthStencilState->depthTestEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthWriteEnable ? 1ull : 0ull);
            key = combine(key, ci.pDepthStencilState->depthCompareOp);
        }

        if (ownsFO && ci.pColorBlendState && ci.pColorBlendState->pAttachments)
        {
            key = combine(key, ci.pColorBlendState->attachmentCount);
            for (uint32_t a = 0; a < ci.pColorBlendState->attachmentCount; ++a)
            {
                const auto& att = ci.pColorBlendState->pAttachments[a];
                key = combine(key, att.blendEnable ? 1ull : 0ull);
                key = combine(key, att.colorWriteMask);
            }
        }

        // Linked pipelines: fold in the libraries' keys (a linked final has no
        // stages / vertex input of its own, so without this all finals collide).
        if (isLinked)
            key = combine(key, libKey);

        if (key == 0)
            key = combine(key, 0xDEADBEEFCAFEBABEull);

        e.key = key;

        const bool hasFrag = ownHasFrag || libHasFrag;

        static std::atomic<int> s_infoLogs{ 0 };
        if (s_infoLogs.fetch_add(1) < 1)
            Log("GP pipe=%p kind=%s flags=0x%x owned=0x%x libParts=0x%x stride=%u strideUnknown=%d "
                "vinDyn=%d strideDyn=%d frag=%d key=%llu",
                (void*)e.pipe,
                isLibrary ? (isLinked ? "linked-lib" : "lib") : (isLinked ? "linked" : "full"),
                (unsigned)ci.flags, (unsigned)owned, (unsigned)libParts,
                stride, strideUnknown ? 1 : 0, vinDyn ? 1 : 0, strideDyn ? 1 : 0,
                hasFrag ? 1 : 0, (unsigned long long)key);

        // ------------------------------------------------------------
        // Highlight creation
        // ------------------------------------------------------------
        VkPipeline highlightPipe = VK_NULL_HANDLE;
        VkResult   hr = VK_SUCCESS;
        bool       attempted = false;

        const bool colorBlendOk =
            ownsFO && ci.pColorBlendState &&
            ci.pColorBlendState->attachmentCount > 0 &&
            ci.pColorBlendState->pAttachments;

        const bool strideWanted = (stride == kHighlightStride) || strideUnknown;

        if (!isLinked && colorBlendOk && (isLibrary || (hasFrag && strideWanted)))
        {
            // (A) fragment-output library: always clone (cheap, no shaders).
            //     The stride filter is applied later, when a final pipeline links it.
            // (B) classic monolithic pipeline: clone only when stride matches / is unknown.
            HighlightStorage hs;
            VkGraphicsPipelineCreateInfo hci = BuildHighlightCI(ci, hs);

            attempted = true;
            hr = pOriginalCreateGraphicsPipelines(device, cache, 1, &hci, pAllocator, &highlightPipe);
        }
        else if (isLinked && swapped && !ownsFO &&
            (isLibrary || (hasFrag && strideWanted)))
        {
            // (C) linked pipeline: same libraries, but the fragment-output library is
            //     replaced by its highlight twin. Everything else is shared.
            VkGraphicsPipelineCreateInfo hci = ci;
            hci.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
                VK_PIPELINE_CREATE_DERIVATIVE_BIT);
            hci.basePipelineHandle = VK_NULL_HANDLE;
            hci.basePipelineIndex = -1;

            // Temporarily point the game's lib-info at our array; restored right after.
            auto* mli = const_cast<VkPipelineLibraryCreateInfoKHR*>(li);
            const VkPipeline* savedLibs = mli->pLibraries;
            mli->pLibraries = hlLibs.data();

            attempted = true;
            hr = pOriginalCreateGraphicsPipelines(device, cache, 1, &hci, pAllocator, &highlightPipe);

            mli->pLibraries = savedLibs;
        }

        if (attempted)
        {
            if (hr == VK_SUCCESS && highlightPipe != VK_NULL_HANDLE)
            {
                if (isLibrary) e.lib.highlightLib = highlightPipe;
                else           e.highlight = highlightPipe;

                if (kVerboseKeyLog)
                    Log("CreateGP: created highlight %p for original %p", (void*)highlightPipe, (void*)e.pipe);
            }
            else
            {
                static std::atomic<int> s_failLogs{ 0 };
                if (s_failLogs.fetch_add(1) < 20)
                    Log("highlight create FAILED hr=%d for %p (kind=%s)", (int)hr, (void*)e.pipe,
                        isLibrary ? "lib" : (isLinked ? "linked" : "full"));
            }
        }

        // Library bookkeeping
        if (isLibrary)
        {
            e.lib.parts = isLinked ? (owned | libParts) : owned;
            e.lib.stride = stride;
            e.lib.strideUnknown = strideUnknown;
            e.lib.hasFrag = hasFrag;
            e.lib.key = key;
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
            if (e.isLibrary)
            {
                g_libInfo[e.pipe] = e.lib;

                // handle may have been reused from a former complete pipeline
                g_pipelineStrides.erase(e.pipe);
                g_pipelineKey.erase(e.pipe);
                g_highlightPipelines.erase(e.pipe);
            }
            else
            {
                g_libInfo.erase(e.pipe);

                g_pipelineStrides[e.pipe] = e.stride;
                g_pipelineKey[e.pipe] = e.key;

                if (e.highlight != VK_NULL_HANDLE)
                    g_highlightPipelines[e.pipe] = e.highlight;
                else
                    g_highlightPipelines.erase(e.pipe);   // never keep a stale clone
            }
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

    if (bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS ||
        pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mtx);

    uint64_t key = 0;

    auto it = g_pipelineKey.find(pipeline);
    if (it != g_pipelineKey.end())
        key = it->second;

    g_curKey[cmd] = key;
    g_curPipeline[cmd] = pipeline;

    if (kVerboseKeyLog)
        Log("Bind: cmd=%p pipeline=%p found=%d key=%llu",
            (void*)cmd, (void*)pipeline, it != g_pipelineKey.end(), key);
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

void VKAPI_CALL DetourVkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t idxCount, uint32_t instCount, uint32_t firstIdx, int32_t vtxOff, uint32_t firstInst) {

    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexed");
        loggedOnce = true;
    }


    uint64_t   key = 0;
    VkPipeline original = VK_NULL_HANDLE;
    VkPipeline highlight = VK_NULL_HANDLE;

    uint32_t dstride = 0;
    uint32_t pstride = 0;
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        auto kit = g_curKey.find(cmd);
        if (kit != g_curKey.end())
            key = kit->second;

        auto pit = g_curPipeline.find(cmd);
        if (pit != g_curPipeline.end())
        {
            original = pit->second;

            auto hit = g_highlightPipelines.find(original);
            if (hit != g_highlightPipelines.end())
                highlight = hit->second;
        }

        auto sit = g_cmdBufStride.find(cmd);
        if (sit != g_cmdBufStride.end())
            dstride = sit->second;

        if (dstride == 0 && original != VK_NULL_HANDLE)
        {
            auto pst = g_pipelineStrides.find(original);
            if (pst != g_pipelineStrides.end())
                pstride = pst->second;
        }
    }
    const uint32_t stride = dstride ? dstride : pstride; //model recognition option 1
    const uint32_t shortkey = static_cast<uint32_t>(key % 100); //model recognition option 2
    //Log("1 dstride == %d && pstride == %d", dstride, pstride);
    
    //if (dstride == 0 && pstride == 0)
    //{
        //Log(
            //"DrawIndexed: cmd=%p original=%p highlight=%p key=%llu "
            //"dstride=0 pstride=0 idxCount=%u",
            //(void*)cmd,
            //(void*)original,
            //(void*)highlight,
            //(unsigned long long)key,
            //idxCount);
    //}
    

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
    if (stride > 0 && stride == countnum) //stride 40 = models in valheim
    {
        //return;
        if (found && localState.hasViewport) {

            // Apply hack
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


    // Color (pipeline method)
    const bool shouldHighlight = original != VK_NULL_HANDLE && highlight != VK_NULL_HANDLE && stride == countnum; //stride 40 = models in valheim
    if (!shouldHighlight)
    {
        pOriginalCmdDrawIndexed(cmd,idxCount,instCount,firstIdx,vtxOff,firstInst);
        return;
    }
    // Highlight draw
    pOriginalCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,highlight);
    pOriginalCmdDrawIndexed(cmd,idxCount,instCount,firstIdx,vtxOff,firstInst);
    // Restores application pipeline (except don't because it will erase color)
    //pOriginalCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,original);


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
    /*
    uint64_t   key = 0;
    VkPipeline original = VK_NULL_HANDLE;
    VkPipeline highlight = VK_NULL_HANDLE;

    uint32_t dstride = 0;
    uint32_t pstride = 0;
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        auto kit = g_curKey.find(cmd);
        if (kit != g_curKey.end())
            key = kit->second;

        auto pit = g_curPipeline.find(cmd);
        if (pit != g_curPipeline.end())
        {
            original = pit->second;

            auto hit = g_highlightPipelines.find(original);
            if (hit != g_highlightPipelines.end())
                highlight = hit->second;
        }

        auto sit = g_cmdBufStride.find(cmd);
        if (sit != g_cmdBufStride.end())
            dstride = sit->second;

        if (dstride == 0 && original != VK_NULL_HANDLE)
        {
            auto pst = g_pipelineStrides.find(original);
            if (pst != g_pipelineStrides.end())
                pstride = pst->second;
        }
    }
    const uint32_t istride = dstride ? dstride : pstride; //model recognition option 1
    const uint32_t shortkey = static_cast<uint32_t>(key % 100); //model recognition option 2
    //Log("2 dstride == %d && pstride == %d", dstride, pstride);


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
    if (stride > 0 && istride == countnum) //stride 40 = models in valheim
    {
        if (found && localState.hasViewport) {

            // Apply hack
            const VkViewport originalVp = localState.currentViewport;
            VkViewport hVp = originalVp;

            // Depth range adjustment
            hVp.minDepth = reversedDepth ? 0.0f : 0.9f;
            hVp.maxDepth = reversedDepth ? 0.1f : 1.0f;

            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &hVp);

            // Draw the hacked version
            pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);

            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &originalVp);
        }
    }


    // Color (pipeline method)
    const bool shouldHighlight = original != VK_NULL_HANDLE && highlight != VK_NULL_HANDLE && istride == countnum; //stride 40 = models in valheim
    if (!shouldHighlight)
    {
        pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
        return;
    }
    // Highlight draw
    pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, highlight);
    pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
    // Restores application pipeline (except don't because it will erase color)
    //pOriginalCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,original);
    */
    return pOriginalCmdDrawIndexedIndirect(cmd, buffer, offset, drawCount, stride);
}

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

//===================================================================================================//

static PFN_vkDestroyPipeline pOriginalDestroyPipeline = nullptr;

void VKAPI_CALL DetourVkDestroyPipeline(
    VkDevice device,
    VkPipeline pipeline,
    const VkAllocationCallbacks* pAllocator)
{
    static bool loggedOnce = false;
    if (!loggedOnce)
    {
        Log("DetourVkDestroyPipeline");
        loggedOnce = true;
    }

    VkPipeline clone = VK_NULL_HANDLE;

    if (pipeline != VK_NULL_HANDLE)
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        auto it = g_highlightPipelines.find(pipeline);
        if (it != g_highlightPipelines.end())
        {
            clone = it->second;
            g_highlightPipelines.erase(it);
        }

        g_pipelineStrides.erase(pipeline);
        g_pipelineKey.erase(pipeline);
    }

    // Destroy the application's pipeline.
    pOriginalDestroyPipeline(device, pipeline, pAllocator);

    // IMPORTANT:
    // This is only safe if you know the GPU is no longer using clone.
    if (clone != VK_NULL_HANDLE)
    {
        pOriginalDestroyPipeline(device, clone, pAllocator);
    }
}

//===================================================================================================//

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
        { "vkDestroyPipeline",         (void*)DetourVkDestroyPipeline,         (void**)&pOriginalDestroyPipeline },
        
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

//===================================================================================================//

DWORD WINAPI HookThread(LPVOID)
{
    Log("Hook Thread Started");

    HMODULE hVulkan = nullptr;
    while (!(hVulkan = GetModuleHandleA("vulkan-1.dll")))
        Sleep(1);

    if (MH_Initialize() != MH_OK)
        return 1;

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