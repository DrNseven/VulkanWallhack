// Vulkan Hook/Wallhack
// Bruteforce stride (By pressing the keys: . and ,)
// Toggle wallhack = F1
// Toggle color = F3
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

#include "main.h" //our custom fragment shader, ect.

//===================================================================================================//

// --- Globals ---
int countstride = 40; //40 = valheim, 48 = zombie army 4: dead war(reversedDepth=true), 28 = deadlock
std::atomic<bool> wallhack{ true };
std::atomic<bool> colorhack{ false };
std::atomic<bool> logvalues{ false };

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
typedef void (VKAPI_PTR* PFN_CmdSetViewportWithCount)(VkCommandBuffer cmd, uint32_t count, const VkViewport* pVp);
typedef void (VKAPI_PTR* PFN_vkCmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexed)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexedIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndirectCount)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexedIndirectCount)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdBindDescriptorSets)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
typedef VkResult(VKAPI_PTR* PFN_vkCreateShaderModule)(VkDevice device,const VkShaderModuleCreateInfo* pCreateInfo,const VkAllocationCallbacks* pAllocator,VkShaderModule* pModule);
typedef void (VKAPI_PTR* PFN_vkCmdSetVertexInputEXT)(VkCommandBuffer commandBuffer,uint32_t vertexBindingDescriptionCount,const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);
typedef VkResult(VKAPI_PTR* PFN_vkResetCommandBuffer)(VkCommandBuffer, VkCommandBufferResetFlags);
typedef void     (VKAPI_PTR* PFN_vkFreeCommandBuffers)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);


// Typedefs
PFN_vkGetDeviceProcAddr pOriginalGetDeviceProcAddr = nullptr;
PFN_vkGetInstanceProcAddr pOriginalGetInstanceProcAddr = nullptr;
PFN_vkCreateGraphicsPipelines pOriginalCreateGraphicsPipelines = nullptr;
PFN_vkCmdBindPipeline         pOriginalCmdBindPipeline = nullptr;
PFN_vkCmdSetViewport_Custom pOriginalCmdSetViewport = nullptr;
PFN_vkCmdSetViewportWithCount pOriginalCmdSetViewportWithCount = nullptr;
PFN_vkCmdDraw pOriginalCmdDraw = nullptr;
PFN_vkCmdDrawIndexed pOriginalCmdDrawIndexed = nullptr;
PFN_vkCmdDrawIndirect pOriginalCmdDrawIndirect = nullptr;
PFN_vkCmdDrawIndexedIndirect pOriginalCmdDrawIndexedIndirect = nullptr;
PFN_vkCmdDrawIndirectCount pOriginalCmdDrawIndirectCount = nullptr;
PFN_vkCmdDrawIndexedIndirectCount pOriginalCmdDrawIndexedIndirectCount = nullptr;
PFN_vkCmdBindDescriptorSets pOriginalCmdBindDescriptorSets = nullptr;
PFN_vkCreateShaderModule pOriginalCreateShaderModule = nullptr;
PFN_vkCmdSetVertexInputEXT pOriginalCmdSetVertexInputEXT = nullptr;
PFN_vkResetCommandBuffer  pOriginalResetCommandBuffer = nullptr;
PFN_vkFreeCommandBuffers  pOriginalFreeCommandBuffers = nullptr;

//===================================================================================================//

// Toggle for the diagnostic Log() calls below
static constexpr bool kVerboseKeyLog = false;

// Viewport, Command Buffer State
struct CmdState {
    VkViewport currentViewport{};
    uint32_t   firstViewport = 0;
    bool       hasViewport = false;
};

std::unordered_map<VkCommandBuffer, CmdState> cmdStates;
std::shared_mutex statesMtx;

void RemoveCmdState(VkCommandBuffer cmd)
{
    if (!cmd) return;
    std::unique_lock<std::shared_mutex> lock(statesMtx);
    //std::unique_lock lock(statesMtx);
    cmdStates.erase(cmd);
}

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


// One module per device, intentionally never destroyed
static std::mutex                  g_redFragMtx;
static std::unordered_map<VkDevice, VkShaderModule> g_redFragMods;

static VkShaderModule GetRedFragModule(VkDevice device)
{
    //Log("GetRedFragModule enter, pOriginal = %p, device = %p", (void*)pOriginalCreateShaderModule, (void*)device);

    if (!pOriginalCreateShaderModule)
    {
        //Log("GetRedFragModule: NULL original – abort");
        return VK_NULL_HANDLE;
    }

    //Log("GetRedFragModule: locking");
    std::lock_guard<std::mutex> lk(g_redFragMtx);
    //Log("GetRedFragModule: locked");

    auto it = g_redFragMods.find(device);
    //Log("GetRedFragModule: after find");

    if (it != g_redFragMods.end())
    {
        //Log("GetRedFragModule: cache hit %p", (void*)it->second);
        return it->second;
    }

    //Log("GetRedFragModule: creating new module");

    VkShaderModuleCreateInfo mci{};
    mci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mci.codeSize = sizeof(CustomFragmentShader);
    mci.pCode = CustomFragmentShader;

    VkShaderModule m = VK_NULL_HANDLE;

    //Log("GetRedFragModule: calling original CreateShaderModule");
    VkResult res = pOriginalCreateShaderModule(device, &mci, nullptr, &m);
    //Log("GetRedFragModule: original returned %d, module = %p", (int)res, (void*)m);

    if (res != VK_SUCCESS)
    {
        Log("GetRedFragModule: FAILED");
        m = VK_NULL_HANDLE;
    }

    g_redFragMods[device] = m;
    return m;
}

// ---- shared bookkeeping ------------------------------------------------------
struct LibInfo
{
    VkGraphicsPipelineLibraryFlagsEXT parts = 0;       // GPL parts this pipeline contains
    uint32_t   stride = 0;                             // from the vertex-input part (0 if dynamic)
    bool       strideUnknown = false;                  // vertex input / stride is dynamic
    bool       hasFrag = false;                        // contains a fragment shader stage
    uint64_t   key = 0;                                // hash of the state this part owns
    VkPipeline highlightLib = VK_NULL_HANDLE;          // highlight twin of this library
    VkGraphicsPipelineLibraryFlagsEXT twinParts = 0;   // which parts the twin actually changed (FS / FO)

    // depth/stencil setup of the fragment-shader part (diagnostics only; -1 = unknown)
    int  depthTest = -1, depthOp = -1, depthWrite = -1, stencilTest = -1;
    bool depthDyn = false;                             // any depth/stencil state is dynamic
};

std::unordered_map<VkPipeline, LibInfo> g_libInfo;     // guarded by g_mtx

static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplVI = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplPre = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplFS = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplFO = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
static constexpr VkGraphicsPipelineLibraryFlagsEXT kGplAll = kGplVI | kGplPre | kGplFS | kGplFO;

// Owns the memory a highlight create-info points into. Must outlive the create call.
struct HighlightStorage
{
    std::vector<VkPipelineShaderStageCreateInfo>     stages;
    std::vector<VkPipelineColorBlendAttachmentState> attachments;
    VkPipelineColorBlendStateCreateInfo              blend{};
    VkPipelineDepthStencilStateCreateInfo            depth{};
};

// Builds the highlight create-info for whatever parts `owned` covers:
//   fragment-shader part  -> fragment stage replaced by the flat-red shader
//   fragment-output part  -> blending off; RT0 writes RGBA, other RTs write nothing
// Returns the mask of parts that were really modified (kGplFS and/or kGplFO), 0 if none.
static VkGraphicsPipelineLibraryFlagsEXT BuildHighlightCI(
    const VkGraphicsPipelineCreateInfo& ci,
    VkGraphicsPipelineLibraryFlagsEXT owned,
    VkShaderModule red,
    HighlightStorage& hs,
    VkGraphicsPipelineCreateInfo& out)
{
    VkGraphicsPipelineLibraryFlagsEXT modified = 0;

    out = ci;
    out.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
        VK_PIPELINE_CREATE_DERIVATIVE_BIT);
    out.basePipelineHandle = VK_NULL_HANDLE;
    out.basePipelineIndex = -1;

    // ---- fragment shader: replace the fragment stage ----
    if ((owned & kGplFS) && ci.pStages && ci.stageCount > 0 && red != VK_NULL_HANDLE)
    {
        hs.stages.assign(ci.pStages, ci.pStages + ci.stageCount);
        for (auto& st : hs.stages)
        {
            if (st.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                st.pNext = nullptr;
                st.flags = 0;
                st.module = red;
                st.pName = "main";
                st.pSpecializationInfo = nullptr;
                modified |= kGplFS;
            }
        }
        if (modified & kGplFS)
            out.pStages = hs.stages.data();
    }

    // ---- depth/stencil: the highlight is an OVERLAY drawn after the game's own draw ----
    // The original draw already wrote depth, so the overlay must pass on equal depth,
    // and must not write depth or stencil. 
    if ((owned & kGplFS) && ci.pDepthStencilState)
    {
        hs.depth = *ci.pDepthStencilState;

        static std::atomic<int> s_depthLogs{ 0 };
        if (s_depthLogs.fetch_add(1) < 1)//10
            Log("highlight depth: test=%d op=%d write=%d",
                (int)ci.pDepthStencilState->depthTestEnable,
                (int)ci.pDepthStencilState->depthCompareOp,
                (int)ci.pDepthStencilState->depthWriteEnable);

        // Keep the game's depth DIRECTION and only make the test inclusive (pass on equal).
        // Unity on Vulkan uses reversed-Z (GREATER_OR_EQUAL), so forcing LESS_OR_EQUAL
        // would pass for everything FARTHER than the stored depth, i.e. behind walls.
        if (hs.depth.depthTestEnable)
        {
            if (hs.depth.depthCompareOp == VK_COMPARE_OP_LESS)
                hs.depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            else if (hs.depth.depthCompareOp == VK_COMPARE_OP_GREATER)
                hs.depth.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
            // LESS_OR_EQUAL / GREATER_OR_EQUAL / EQUAL / others stay exactly as the game has them
        }
        hs.depth.depthWriteEnable = VK_FALSE;

        // Keep the game's stencil TEST (it may be what hides the model behind things),
        // but make sure the overlay never changes stencil values.
        hs.depth.front.failOp = hs.depth.front.passOp = hs.depth.front.depthFailOp = VK_STENCIL_OP_KEEP;
        hs.depth.back.failOp = hs.depth.back.passOp = hs.depth.back.depthFailOp = VK_STENCIL_OP_KEEP;
        hs.depth.front.writeMask = 0;
        hs.depth.back.writeMask = 0;
        out.pDepthStencilState = &hs.depth;
    }

    /*
   //VK_COMPARE_OP_ALWAYS wallhack works too, better for fps, BUT does not punch though all walls
   // ---- depth/stencil: make the highlight a true wallhack overlay ----
   if ((owned & kGplFS) && ci.pDepthStencilState)
   {
       hs.depth = *ci.pDepthStencilState;

       // Force always-pass + no depth write
       hs.depth.depthTestEnable = VK_TRUE;               // keep test enabled so the op is used
       hs.depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;  //
       hs.depth.depthWriteEnable = VK_FALSE;

       // Never touch stencil
       hs.depth.front.failOp = hs.depth.front.passOp = hs.depth.front.depthFailOp = VK_STENCIL_OP_KEEP;
       hs.depth.back.failOp = hs.depth.back.passOp = hs.depth.back.depthFailOp = VK_STENCIL_OP_KEEP;
       hs.depth.front.writeMask = 0;
       hs.depth.back.writeMask = 0;

       out.pDepthStencilState = &hs.depth;
   }
   */

   /*
   //same effect as above
   if ((owned & kGplFS) && ci.pDepthStencilState)
   {
       hs.depth = *ci.pDepthStencilState;

       hs.depth.depthTestEnable = VK_FALSE;
       hs.depth.depthWriteEnable = VK_FALSE;
       hs.depth.stencilTestEnable = VK_FALSE;

       out.pDepthStencilState = &hs.depth;
   }
   */   

   /*
    //This version has bug that disables depth too, wallhack effect
    // The original draw already wrote depth, so the overlay must pass on equal depth,
    // must not write depth, and must not be rejected by the game's stencil setup.
    if ((owned & kGplFS) && ci.pDepthStencilState)
    {
        hs.depth = *ci.pDepthStencilState;
        if (hs.depth.depthTestEnable)
            hs.depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        hs.depth.depthWriteEnable = VK_FALSE;
        hs.depth.stencilTestEnable = VK_FALSE;
        out.pDepthStencilState = &hs.depth;
    }
    */

    // ---- fragment output: overlay writes RGB of RT0 only ----
    // Every other attachment (normals, motion vectors, masks, ...) and the alpha
    // channel keep what the game's own draw wrote, so nothing is left with holes.
    if ((owned & kGplFO) && ci.pColorBlendState &&
        ci.pColorBlendState->attachmentCount > 0 && ci.pColorBlendState->pAttachments)
    {
        hs.attachments.assign(ci.pColorBlendState->attachmentCount, VkPipelineColorBlendAttachmentState{});
        for (size_t a = 0; a < hs.attachments.size(); ++a)
        {
            VkPipelineColorBlendAttachmentState& att = hs.attachments[a];
            att.blendEnable = VK_FALSE;
            att.colorWriteMask = (a == 0)
                ? (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT)
                : 0;
        }

        hs.blend = *ci.pColorBlendState;
        hs.blend.logicOpEnable = VK_FALSE;
        hs.blend.pAttachments = hs.attachments.data();
        out.pColorBlendState = &hs.blend;
        modified |= kGplFO;
    }

    return modified;
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

    // Create the original pipelines first
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
        bool depthDyn = false;

        if (ci.pDynamicState && ci.pDynamicState->pDynamicStates)
        {
            for (uint32_t d = 0; d < ci.pDynamicState->dynamicStateCount; ++d)
            {
                const VkDynamicState s = ci.pDynamicState->pDynamicStates[d];
                if (s == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE) strideDyn = true;
                if (s == VK_DYNAMIC_STATE_VERTEX_INPUT_EXT)            vinDyn = true;
                if (s == VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE || s == VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE ||
                    s == VK_DYNAMIC_STATE_DEPTH_COMPARE_OP || s == VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE ||
                    s == VK_DYNAMIC_STATE_STENCIL_OP)
                    depthDyn = true;
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
        // Info from linked libraries (stride, key, parts, frag stage)
        // and build the list with each library swapped for its highlight twin
        // ------------------------------------------------------------
        VkGraphicsPipelineLibraryFlagsEXT libParts = 0;
        VkGraphicsPipelineLibraryFlagsEXT swappedParts = 0;   // FS/FO parts the twins really changed
        uint64_t libKey = 0;
        uint32_t libStride = 0;
        bool     libStrideUnknown = false;
        bool     libHasFrag = false;
        bool     swapped = false;
        int      mTest = -1, mOp = -1, mWrite = -1, mStencil = -1;   // depth info from the FS library
        bool     mDepthDyn = false;
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

                if (L.parts & kGplFS)
                {
                    mTest = L.depthTest;  mOp = L.depthOp;
                    mWrite = L.depthWrite; mStencil = L.stencilTest;
                    mDepthDyn = L.depthDyn;
                }

                if (L.parts & kGplVI)
                {
                    libStride = L.stride;
                    libStrideUnknown = L.strideUnknown;
                }

                if (L.highlightLib != VK_NULL_HANDLE && L.twinParts != 0)
                {
                    lib = L.highlightLib;
                    swappedParts |= L.twinParts;
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

        // Linked pipelines: fold in the libraries' keys (a linked final has no stages / vertex input of its own, so without this all finals collide).
        if (isLinked)
            key = combine(key, libKey);

        if (key == 0)
            key = combine(key, 0xDEADBEEFCAFEBABEull);

        e.key = key;

        const bool hasFrag = ownHasFrag || libHasFrag;

        // depth/stencil setup (own if this create info owns the FS part, else from the linked FS library)
        int dTest = mTest, dOp = mOp, dWrite = mWrite, dStencil = mStencil;
        bool dDyn = mDepthDyn;
        if (ownsFS && ci.pDepthStencilState)
        {
            dTest = ci.pDepthStencilState->depthTestEnable ? 1 : 0;
            dOp = (int)ci.pDepthStencilState->depthCompareOp;
            dWrite = ci.pDepthStencilState->depthWriteEnable ? 1 : 0;
            dStencil = ci.pDepthStencilState->stencilTestEnable ? 1 : 0;
            dDyn = depthDyn;
        }

        // Does this fragment-shader part really reject hidden pixels? A pass with depth test
        // off / ALWAYS draws through everything, so an overlay on it would be visible through
        // walls. Dynamic depth state can't be judged here, so it is allowed.
        const bool realDepthTest =
            depthDyn ||
            (ci.pDepthStencilState && ci.pDepthStencilState->depthTestEnable &&
                ci.pDepthStencilState->depthCompareOp != VK_COMPARE_OP_ALWAYS &&
                ci.pDepthStencilState->depthCompareOp != VK_COMPARE_OP_NEVER &&
                ci.pDepthStencilState->depthCompareOp != VK_COMPARE_OP_NOT_EQUAL);

        static std::atomic<int> s_infoLogs{ 0 };
        if (s_infoLogs.fetch_add(1) < 1)
            Log("GP pipe=%p kind=%s flags=0x%x owned=0x%x libParts=0x%x stride=%u strideUnknown=%d "
                "vinDyn=%d strideDyn=%d frag=%d att=%u key=%llu",
                (void*)e.pipe,
                isLibrary ? (isLinked ? "linked-lib" : "lib") : (isLinked ? "linked" : "full"),
                (unsigned)ci.flags, (unsigned)owned, (unsigned)libParts,
                stride, strideUnknown ? 1 : 0, vinDyn ? 1 : 0, strideDyn ? 1 : 0,
                hasFrag ? 1 : 0,
                (ownsFO && ci.pColorBlendState) ? ci.pColorBlendState->attachmentCount : 0u,
                (unsigned long long)key);

        // ------------------------------------------------------------
        // Highlight creation (flat-red fragment shader, blending off)
        // ------------------------------------------------------------
        VkPipeline highlightPipe = VK_NULL_HANDLE;
        VkResult   hr = VK_SUCCESS;
        bool       attempted = false;
        VkGraphicsPipelineLibraryFlagsEXT twinParts = 0;
        //Log("1");
        //const bool strideWanted = true;
        const bool strideWanted = (stride >= 1) || strideUnknown; //<--- stride >= 1 better to use the models stride like 40, but need to bruteforce this first in drawindexed
        //Log("2");

        //Log("isLinked == %d && ownsFS == %d && realDepthTest == %d && isLibrary == %d && ownsFO == %d && hasFrag == %d",
            //isLinked, ownsFS, realDepthTest, isLibrary, ownsFO, hasFrag);

        if (!isLinked && (!ownsFS || realDepthTest) && (isLibrary ? (ownsFS || ownsFO) : (hasFrag && strideWanted)))
        //if (!isLinked && (isLibrary ? (ownsFS || ownsFO) : (hasFrag && strideWanted)))
        {
            //Log("3");

            //Log("3a – calling GetRedFragModule");
            VkShaderModule red = ownsFS ? GetRedFragModule(device) : VK_NULL_HANDLE;
            //Log("3b – red = %p", (void*)red);

            //Log("3c – calling BuildHighlightCI");
            HighlightStorage hs;
            VkGraphicsPipelineCreateInfo hci;
            twinParts = BuildHighlightCI(ci, owned, red, hs, hci);
            //Log("3d – twinParts = 0x%x", (unsigned)twinParts);

            const bool complete = isLibrary
                ? (twinParts != 0)
                : ((twinParts & (kGplFS | kGplFO)) == (kGplFS | kGplFO));
            //Log("3e – complete = %d", (int)complete);

            if (complete)
            {
                //Log("3f – calling CreateGraphicsPipelines for highlight");
                attempted = true;
                hr = pOriginalCreateGraphicsPipelines(device, cache, 1, &hci, pAllocator, &highlightPipe);
                //Log("3g – hr = %d  highlight = %p", (int)hr, (void*)highlightPipe);
            }
            //Log("3h – finished");
        }

        else if (isLinked && swapped && !ownsFS && !ownsFO &&
            (isLibrary || (hasFrag && strideWanted &&
                (swappedParts & (kGplFS | kGplFO)) == (kGplFS | kGplFO))))
        {
            //Log("4");
            // Safe copy – never mutate the application's create-info
            VkGraphicsPipelineCreateInfo hci = ci;
            hci.flags &= ~(VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT |
                VK_PIPELINE_CREATE_DERIVATIVE_BIT);
            hci.basePipelineHandle = VK_NULL_HANDLE;
            hci.basePipelineIndex = -1;

            // Local copy of the library info
            VkPipelineLibraryCreateInfoKHR localLib = *li;
            localLib.pLibraries = hlLibs.data();          // our vector stays alive for the call

            // Re-wire pNext so the local lib info is used
            // (simple case – assumes li is directly in the pNext chain)
            // For a more robust version you can rebuild the whole pNext chain,
            // but this is enough for the common case.
            hci.pNext = &localLib;

            // If the original pNext contained more structures you care about,
            // you would need to copy them too. For most games the library info
            // is the only relevant one here.

            attempted = true;
            twinParts = swappedParts;
            hr = pOriginalCreateGraphicsPipelines(device, cache, 1, &hci, pAllocator, &highlightPipe);
        }

        //Log("5");
        if (attempted)
        {
            //Log("6");
            if (hr == VK_SUCCESS && highlightPipe != VK_NULL_HANDLE)
            {
                //Log("7");
                if (isLibrary)
                {
                    e.lib.highlightLib = highlightPipe;
                    e.lib.twinParts = twinParts;
                }
                else
                {
                    e.highlight = highlightPipe;

                    static std::atomic<int> s_modelLogs{ 0 };
                    if (s_modelLogs.fetch_add(1) < 1)
                        Log("HL model pipe=%p stride=%u strideUnknown=%d depthTest=%d op=%d write=%d "
                            "stencil=%d depthDynamic=%d",
                            (void*)e.pipe, stride, strideUnknown ? 1 : 0,
                            dTest, dOp, dWrite, dStencil, dDyn ? 1 : 0);
                }

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
            e.lib.depthTest = dTest;   e.lib.depthOp = dOp;
            e.lib.depthWrite = dWrite; e.lib.stencilTest = dStencil;
            e.lib.depthDyn = dDyn;
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

    if (pVp && count > 0 && cmd)
    {
        std::unique_lock<std::shared_mutex> lock(statesMtx);
        CmdState& state = cmdStates[cmd];
        state.currentViewport = pVp[0];   // we only care about the first
        state.firstViewport = first;
        state.hasViewport = true;
    }

    if (pOriginalCmdSetViewport)
        pOriginalCmdSetViewport(cmd, first, count, pVp);
}

//===================================================================================================//

void VKAPI_CALL DetourVkCmdSetViewportWithCount(VkCommandBuffer cmd, uint32_t count, const VkViewport* pVp)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdSetViewportWithCount");
        loggedOnce = true;
    }

    if (pVp && count > 0 && cmd)
    {
        std::unique_lock<std::shared_mutex> lock(statesMtx);
        CmdState& st = cmdStates[cmd];
        st.currentViewport = pVp[0];   // we only care about the first
        st.firstViewport = 0;
        st.hasViewport = true;
    }

    if (pOriginalCmdSetViewportWithCount)
    pOriginalCmdSetViewportWithCount(cmd, count, pVp);
}

//===================================================================================================//

void VKAPI_CALL DetourVkCmdSetVertexInputEXT(
    VkCommandBuffer cmd,
    uint32_t bindingCount, const VkVertexInputBindingDescription2EXT* pBindings,
    uint32_t attrCount, const VkVertexInputAttributeDescription2EXT* pAttrs)
{
    static std::atomic<bool> s_logged{ false };
    if (!s_logged.exchange(true))
        Log("DetourVkCmdSetVertexInputEXT");

    if (pBindings && bindingCount > 0)
    {
        // build outside the lock
        VertexInputState st;
        st.bindingCount = bindingCount;
        st.bindings.assign(pBindings, pBindings + bindingCount);
        st.attributeCount = pAttrs ? attrCount : 0;
        if (pAttrs && attrCount > 0)
            st.attributes.assign(pAttrs, pAttrs + attrCount);

        const uint32_t stride = pBindings[0].stride;

        {
            std::lock_guard<std::mutex> lock(g_mtx);     // only cheap writes under the lock
            g_cmdBufStride[cmd] = stride;
            g_cmdBufVertexInput[cmd] = std::move(st);
        }
    }

    // never call into the driver while holding g_mtx
    pOriginalCmdSetVertexInputEXT(cmd, bindingCount, pBindings, attrCount, pAttrs);
}

//===================================================================================================//

//Viewport wallhack punches through all walls, but worse for fps
void VKAPI_CALL DetourVkCmdDrawIndexed(VkCommandBuffer cmd, uint32_t idxCount, uint32_t instCount,
    uint32_t firstIdx, int32_t vtxOff, uint32_t firstInst)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexed");
        loggedOnce = true;
    }
    
    VkPipeline original = VK_NULL_HANDLE;
    VkPipeline highlight = VK_NULL_HANDLE;
    uint32_t   dstride = 0;
    uint32_t   pstride = 0;
    uint64_t   key = 0;

    {
        std::lock_guard<std::mutex> lock(g_mtx);
        auto pit = g_curPipeline.find(cmd);
        if (pit != g_curPipeline.end())
        {
            original = pit->second;
            auto hit = g_highlightPipelines.find(original);
            if (hit != g_highlightPipelines.end())
                highlight = hit->second;
            auto pst = g_pipelineStrides.find(original);
            if (pst != g_pipelineStrides.end())
                pstride = pst->second;
        }
        auto sit = g_cmdBufStride.find(cmd);
        if (sit != g_cmdBufStride.end())
            dstride = sit->second;
        auto kit = g_curKey.find(cmd);
        if (kit != g_curKey.end())
            key = kit->second;
    }
    const uint32_t stride = dstride ? dstride : pstride; //model recognition option 1
    const uint32_t shortkey = static_cast<uint32_t>(key % 100); //model recognition option 2


    // Bruteforce stride (By pressing the keys period(.) and comma(,), minus(-) will reset it -1 again.
    if (stride == countstride)
    if(logvalues) //press F5 to log, press F5 again to stop logging
        Log("stride == %d && shortkey == %d && idxCount == %d", stride, shortkey, idxCount);


    // Model recognition
    if (stride != countstride) //40 = valheim, 48 = zombie army 4: dead war(reversedDepth=true), 28,56? = deadlock
    {
        if (pOriginalCmdDrawIndexed)
            pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);
        return;
    }

    // Capture viewport
    bool hasVp = false;
    CmdState localState{};
    {
        std::shared_lock<std::shared_mutex> lock(statesMtx);
        auto it = cmdStates.find(cmd);
        if (it != cmdStates.end() && it->second.hasViewport)
        {
            localState = it->second;
            hasVp = true;
        }
    }


    bool wantColor = (original != VK_NULL_HANDLE && highlight != VK_NULL_HANDLE);
    bool wantWall = hasVp;

    if (!wallhack)  wantWall = false;
    if (!colorhack) wantColor = false;

    // ------------------------------------------------------------------
    // 1) Normal draw – keeps depth buffer correct for the rest of the frame
    // ------------------------------------------------------------------
    if (pOriginalCmdDrawIndexed)
        pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

    // ------------------------------------------------------------------
    // 2) + 3) Wallhack solid + Colour  (both use modified viewport)
    // ------------------------------------------------------------------
    if (wantWall || wantColor)
    {
        // Apply wallhack viewport once for the remaining passes
        if (wantWall && pOriginalCmdSetViewport)
        {
            VkViewport hVp = localState.currentViewport;
            constexpr bool reversedDepth = false;   // set true if game uses reversed-Z (zombie army 4)
            hVp.minDepth = reversedDepth ? 0.0f : 0.9f;
            hVp.maxDepth = reversedDepth ? 0.1f : 1.0f;
            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &hVp);
        }

        // 2) Solid wallhack pass
        if (wantWall && pOriginalCmdDrawIndexed)
            pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

        // 3) Colour pass 
        if (wantColor)
        {
            if (pOriginalCmdBindPipeline)
                pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, highlight);

            if (pOriginalCmdDrawIndexed)
                pOriginalCmdDrawIndexed(cmd, idxCount, instCount, firstIdx, vtxOff, firstInst);

            if (pOriginalCmdBindPipeline)
                pOriginalCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, original);
        }

        // Restore original viewport
        if (wantWall && pOriginalCmdSetViewport)
            pOriginalCmdSetViewport(cmd, localState.firstViewport, 1, &localState.currentViewport);
    }
}

//===================================================================================================//

void VKAPI_CALL DetourVkCmdDrawIndexedIndirect(VkCommandBuffer cmd, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
    static bool loggedOnce = false;
    if (!loggedOnce) {
        Log("DetourVkCmdDrawIndexedIndirect");
        loggedOnce = true;
    }
 
    //if game is drawing models in this fuction, paste code from DrawIndexed here and rename stride to istride

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

    //Log("hook %s @%p -> %s", name, (void*)fn, MH_StatusToString(s));

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

//===================================================================================================

// Lifetime / cleanup detours
void ClearCmdState(VkCommandBuffer cmd)
{
    if (!cmd) return;

    {
        std::unique_lock<std::shared_mutex> lock(statesMtx);
        cmdStates.erase(cmd);
    }

    std::lock_guard<std::mutex> lock(g_mtx);
    g_cmdBufStride.erase(cmd);
    g_cmdBufVertexInput.erase(cmd);
    g_curKey.erase(cmd);
    g_curPipeline.erase(cmd);
}

VkResult VKAPI_CALL DetourVkResetCommandBuffer(VkCommandBuffer commandBuffer,VkCommandBufferResetFlags flags)
{
    ClearCmdState(commandBuffer);
    return pOriginalResetCommandBuffer
        ? pOriginalResetCommandBuffer(commandBuffer, flags)
        : VK_SUCCESS;
}

void VKAPI_CALL DetourVkFreeCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers)
{
    if (pCommandBuffers)
    {
        for (uint32_t i = 0; i < commandBufferCount; ++i)
            ClearCmdState(pCommandBuffers[i]);
    }

    if (pOriginalFreeCommandBuffers)
        pOriginalFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
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
        { "vkCmdSetViewportWithCount", (void*)DetourVkCmdSetViewportWithCount, (void**)&pOriginalCmdSetViewportWithCount },
        { "vkCmdDrawIndexed",          (void*)DetourVkCmdDrawIndexed,          (void**)&pOriginalCmdDrawIndexed },
        { "vkCmdDrawIndexedIndirect",  (void*)DetourVkCmdDrawIndexedIndirect,  (void**)&pOriginalCmdDrawIndexedIndirect },
        { "vkDestroyPipeline",         (void*)DetourVkDestroyPipeline,         (void**)&pOriginalDestroyPipeline },
        { "vkResetCommandBuffer",      (void*)DetourVkResetCommandBuffer,      (void**)&pOriginalResetCommandBuffer },
        { "vkFreeCommandBuffers",      (void*)DetourVkFreeCommandBuffers,      (void**)&pOriginalFreeCommandBuffers },
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
    bool p1 = false, p2 = false, p3 = false;
    while (true) {
        if (GetAsyncKeyState(VK_OEM_COMMA) & 1) { countstride--; }
        if (GetAsyncKeyState(VK_OEM_PERIOD) & 1) { countstride++; }
        if (GetAsyncKeyState(VK_OEM_MINUS) & 1) { countstride = -1; }

        bool c1 = GetAsyncKeyState(VK_F3) & 0x8000;
        bool c2 = GetAsyncKeyState(VK_F4) & 0x8000;
        bool c3 = GetAsyncKeyState(VK_F5) & 0x8000;

        if (c1 && !p1) wallhack = !wallhack;   
        if (c2 && !p2) colorhack = !colorhack;
        if (c3 && !p3) logvalues = !logvalues;

        p1 = c1; p2 = c2, p3 = c3;
        Sleep(2);
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