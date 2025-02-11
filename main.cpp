//Vulkan Hook 2025

#include <Windows.h>
#include <iostream>
#include <assert.h>
#include <cstdint>  // For uintptr_t
#include <cassert>  // For assert()
#include <cstdio>
#include <vector>
#include <mutex>
#include <unordered_map>
#include "vulkan/vulkan.h"
#include "minhook/include/MinHook.h"

//#include "glm/vec3.hpp" // glm::vec3
//#include "glm/vec4.hpp" // glm::vec4
//#include "glm/mat4x4.hpp" // glm::mat4
//#include "glm/ext/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale
//#include "glm/ext/matrix_clip_space.hpp" // glm::perspective
//#include "glm/ext/scalar_constants.hpp" // glm::pi

#pragma comment(lib, "vulkan/vulkan-1.lib")

//=======================================================================================//

// Use these Globals
VkDevice g_device = VK_NULL_HANDLE; //device
VkCommandPool g_commandPool = VK_NULL_HANDLE;
VkQueue g_graphicsQueue = VK_NULL_HANDLE;
uint32_t g_queueFamilyIndex;
VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
VkGraphicsPipelineCreateInfo g_createInfos;
VkRenderPass g_renderPass = VK_NULL_HANDLE;
VkPipeline zbufferOFFPipeline = VK_NULL_HANDLE;
VkPipeline zbufferONPipeline = VK_NULL_HANDLE;
std::mutex boundPipelineMapMutex;
std::unordered_map<VkCommandBuffer, VkPipeline> boundPipelineMap;
std::unordered_map<VkPipeline, VkPipelineLayout> pipelineLayoutMap;
int dstBinding;
float WindowWidth;
float WindowHeight;
bool initonce = false;
bool initcommandpool = false;
int countnum = -1;

//=======================================================================================//

#include "main.h"

//=======================================================================================//

typedef void (VKAPI_PTR* PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef void (VKAPI_PTR* PFN_vkCmdBindPipeline)(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline);
typedef void (VKAPI_PTR* PFN_vkCmdBeginRenderPass)(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
typedef void (VKAPI_PTR* PFN_VkCmdBindVertexBuffers)(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount,const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);
typedef void (VKAPI_PTR* PFN_vkCmdDraw)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexed)(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef void (VKAPI_PTR* PFN_vkCmdDrawIndexedIndirect)(VkCommandBuffer, VkBuffer, VkDeviceSize, uint32_t, uint32_t);
typedef VkResult(VKAPI_PTR* PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);

PFN_vkGetDeviceQueue oVkGetDeviceQueue = nullptr;
PFN_vkCmdBindPipeline original_vkCmdBindPipeline = nullptr;
PFN_vkCreateGraphicsPipelines original_vkCreateGraphicsPipelines = nullptr;
PFN_vkCmdBeginRenderPass oVkCmdBeginRenderPass = nullptr;
PFN_vkCmdPushConstants oVkCmdPushConstants = nullptr;
PFN_VkCmdBindVertexBuffers oVkCmdBindVertexBuffers = nullptr;
PFN_vkBindBufferMemory oVkBindBufferMemory = nullptr;
PFN_vkQueueSubmit oVkQueueSubmit = nullptr;
PFN_vkCmdBindDescriptorSets oVkCmdBindDescriptorSets = nullptr;
PFN_vkUpdateDescriptorSets oVkUpdateDescriptorSets = nullptr;
PFN_vkBeginCommandBuffer oVkBeginCommandBuffer = nullptr;
PFN_vkCmdDraw oVkCmdDraw = nullptr;
PFN_vkCmdDrawIndexed oVkCmdDrawIndexed = nullptr;
PFN_vkCmdDrawIndirect oVkCmdDrawIndirect = nullptr;
PFN_vkCmdDrawIndexedIndirect oVkCmdDrawIndexedIndirect = nullptr;
PFN_vkEnumeratePhysicalDevices oVkEnumeratePhysicalDevices = nullptr;

//=======================================================================================//

void VKAPI_PTR hkVkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) 
{
	//Log("VkGetDeviceQueue");

	g_device = device; // Store the device handle
	g_graphicsQueue = *pQueue;
	g_queueFamilyIndex = queueFamilyIndex;

	if(!initcommandpool)
	{
		createCommandPool(device, queueFamilyIndex);
		initcommandpool = true;
	}

	oVkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

//=======================================================================================// 

void VKAPI_PTR hkVkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {

	lognospam(10, "VkCmdDrawIndexed");
	if (!initonce) {
		if (g_device == nullptr) {
			Log("g_device not found, injected too late.");
		}

		// zbuffer
		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

		initonce = true;
	}

	//wallhack example
	if (indexCount == 765|| indexCount == 1032||indexCount == 25698|| indexCount == 34494)
	{
		//disable z buffer
		//vkCmdSetDepthCompareOp(commandBuffer, VK_COMPARE_OP_ALWAYS);
		//vkCmdSetDepthWriteEnable(commandBuffer, VK_FALSE);

		vkCmdSetDepthTestEnable(commandBuffer, VK_FALSE);
		vkCmdSetDepthWriteEnable(commandBuffer, VK_FALSE);

		oVkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

		//vkCmdSetDepthCompareOp(commandBuffer, VK_COMPARE_OP_LESS);
		//vkCmdSetDepthWriteEnable(commandBuffer, VK_TRUE);

		vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);
		vkCmdSetDepthWriteEnable(commandBuffer, VK_TRUE);

	}

	//logger for logging indexcount
	if (countnum == indexCount / 100)
		if (GetAsyncKeyState(VK_END) & 1) //press END to log to log.txt
			Log("indexCount == %d", indexCount); //log selected values

	//erase selected textures
	if (countnum == indexCount / 100)
	{
		return;
	}

	//hold down P key until a texture is erased, press END to log values of those textures
	if (GetAsyncKeyState('O') & 1) //-
		countnum--;
	if (GetAsyncKeyState('P') & 1) //+
		countnum++;
	if (GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('9') & 1) //reset, set to -1
		countnum = -1;


	oVkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	lognospam(50, "vkCmdDraw called");
	//Log("[Hooked] vkCmdDraw called! vertexCount: %d, instanceCount: %d\n", vertexCount, instanceCount);

	oVkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	lognospam(50, "VkCmdDrawIndirect");
	//Log("[Hooked] vkCmdDrawIndirect called! drawCount: %d\n", drawCount);

	oVkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride)
{
	lognospam(50, "VkCmdDrawIndexedIndirect called");
	//Log("[Hooked] vkCmdDrawIndexedIndirect called! drawCount: %d\n", drawCount);

	oVkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

//=======================================================================================//

VKAPI_ATTR void VKAPI_CALL Hooked_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
	/*
	if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
		std::lock_guard<std::mutex> lock(boundPipelineMapMutex);
		boundPipelineMap[commandBuffer] = pipeline;
		g_pipelineLayout = pipelineLayoutMap[pipeline];
	}
	*/

	if (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
		boundPipelineMap[commandBuffer] = pipeline;
		g_pipelineLayout = pipelineLayoutMap[pipeline];
	}

	original_vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

//=======================================================================================//

VKAPI_ATTR VkResult VKAPI_CALL Hooked_vkCreateGraphicsPipelines(
	VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
	const VkGraphicsPipelineCreateInfo* pCreateInfos,
	const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines)
{
	VkResult result = original_vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);

	if (result == VK_SUCCESS) {
		for (uint32_t i = 0; i < createInfoCount; ++i) {
			pipelineLayoutMap[pPipelines[i]] = pCreateInfos[i].layout;
			g_pipelineLayout = pCreateInfos[i].layout;
		}
	}

	return result;
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) 
{
	if (pRenderPassBegin != nullptr)
		g_renderPass = pRenderPassBegin->renderPass;

	oVkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdPushConstants(
	VkCommandBuffer commandBuffer,
	VkPipelineLayout layout,
	VkShaderStageFlags stageFlags,
	uint32_t offset,
	uint32_t size,
	const void* pValues)
{
	/*
	if (size == sizeof(glm::mat4))
	{
		memcpy(&g_ViewProjectionMatrix, pValues, sizeof(glm::mat4));
		Log("Captured ViewProjection Matrix from Push Constants.");
	}
	*/

	// Call original function
	oVkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

//=======================================================================================//

std::unordered_map<uint32_t, VkBuffer> g_BoundVertexBuffers;  // Stores bound buffers
std::unordered_map<uint32_t, VkDeviceSize> g_BoundOffsets;   // Stores buffer offsets

void VKAPI_PTR hkVkCmdBindVertexBuffers(
	VkCommandBuffer commandBuffer,
	uint32_t firstBinding,
	uint32_t bindingCount,
	const VkBuffer* pBuffers,
	const VkDeviceSize* pOffsets)
{
	/*
	for (uint32_t i = 0; i < bindingCount; i++) {
		g_BoundVertexBuffers[firstBinding + i] = pBuffers[i];
		g_BoundOffsets[firstBinding + i] = pOffsets[i];

		//Log("Bound Buffer[%d]: %p Offset: %llu", firstBinding + i, pBuffers[i], pOffsets[i]);
	}
	*/

	oVkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

//=======================================================================================//

//std::unordered_map<VkBuffer, VkDeviceMemory> g_VertexBufferMemory;

void VKAPI_PTR hkVkBindBufferMemory(
	VkDevice device,
	VkBuffer buffer,
	VkDeviceMemory memory,
	VkDeviceSize memoryOffset)
{
	//g_VertexBufferMemory[buffer] = memory;
	//Log("Buffer %p is bound to Memory %p", buffer, memory);

	oVkBindBufferMemory(device, buffer, memory, memoryOffset);
}

//=======================================================================================//

void VKAPI_PTR hkVkCmdBindDescriptorSets(
	VkCommandBuffer commandBuffer,
	VkPipelineBindPoint pipelineBindPoint,
	VkPipelineLayout layout,
	uint32_t firstSet,
	uint32_t descriptorSetCount,
	const VkDescriptorSet* pDescriptorSets,
	uint32_t dynamicOffsetCount,
	const uint32_t* pDynamicOffsets)
{

	oVkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet,
		descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

//=======================================================================================//

void VKAPI_PTR hkVkUpdateDescriptorSets(
	VkDevice device,
	uint32_t descriptorWriteCount,
	const VkWriteDescriptorSet* pDescriptorWrites,
	uint32_t descriptorCopyCount,
	const VkCopyDescriptorSet* pDescriptorCopies)
{
	
	oVkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

//=======================================================================================//

VkResult hkVkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{

	return oVkQueueSubmit(queue, submitCount, pSubmits, fence);
}

//=======================================================================================//

VkResult hvkBeginCommandBuffer(VkCommandBuffer commandBuffer,const VkCommandBufferBeginInfo* pBeginInfo)
{

	return oVkBeginCommandBuffer(commandBuffer, pBeginInfo);
}

//=======================================================================================//

VkResult VKAPI_PTR hkVkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pDeviceCount, VkPhysicalDevice* pDevices)
{
	VkResult result = oVkEnumeratePhysicalDevices(instance, pDeviceCount, pDevices);
	if (result == VK_SUCCESS && pDevices && *pDeviceCount > 0)
	{
		g_physicalDevice = pDevices[0]; // Store first device
		//Log("Hooked vkEnumeratePhysicalDevices! Captured PhysicalDevice");
	}
	return result;
}

//=======================================================================================//

// Hook vkCreateQueryPool
typedef VkResult(VKAPI_PTR* PFN_vkCreateQueryPool)(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool);
static PFN_vkCreateQueryPool Original_vkCreateQueryPool = nullptr;

VkResult Hooked_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
	if (pCreateInfo->queryType == VK_QUERY_TYPE_OCCLUSION) {
		// Modify the query pool creation info to use a timestamp query instead.
		VkQueryPoolCreateInfo modifiedCreateInfo = *pCreateInfo; // Copy the original
		modifiedCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP; // Change query type
		modifiedCreateInfo.pipelineStatistics = 0; // Important: clear this

		//Log("Replaced OCCLUSION query with TIMESTAMP query.");
		return Original_vkCreateQueryPool(device, &modifiedCreateInfo, pAllocator, pQueryPool);
	}
	else if (pCreateInfo->queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS && (pCreateInfo->pipelineStatistics & VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT))
	{
		VkQueryPoolCreateInfo modifiedCreateInfo = *pCreateInfo; // Copy the original
		modifiedCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP; // Change query type
		modifiedCreateInfo.pipelineStatistics = 0; // Important: clear this

		//Log("Replaced PIPELINE query with TIMESTAMP query.");
		return Original_vkCreateQueryPool(device, &modifiedCreateInfo, pAllocator, pQueryPool);
	}

	// For other query types, just call the original function.
	return Original_vkCreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

//=======================================================================================//

void HookVulkanFunctions()
{
	HMODULE libVulkan;
	if ((libVulkan = GetModuleHandle(VTEXT("vulkan-1.dll"))) == NULL)
	{
		Log("vulkan-1.dll not found!");
	}

	const char* const methodsNames[] = {
		"vkCreateInstance", "vkDestroyInstance", "vkEnumeratePhysicalDevices", "vkGetPhysicalDeviceFeatures", "vkGetPhysicalDeviceFormatProperties", "vkGetPhysicalDeviceImageFormatProperties",
		"vkGetPhysicalDeviceProperties", "vkGetPhysicalDeviceQueueFamilyProperties", "vkGetPhysicalDeviceMemoryProperties", "vkGetInstanceProcAddr", "vkGetDeviceProcAddr", "vkCreateDevice",
		"vkDestroyDevice", "vkEnumerateInstanceExtensionProperties", "vkEnumerateDeviceExtensionProperties", "vkEnumerateDeviceLayerProperties", "vkGetDeviceQueue", "vkQueueSubmit", "vkQueueWaitIdle",
		"vkDeviceWaitIdle", "vkAllocateMemory", "vkFreeMemory", "vkMapMemory", "vkUnmapMemory", "vkFlushMappedMemoryRanges", "vkInvalidateMappedMemoryRanges", "vkGetDeviceMemoryCommitment",
		"vkBindBufferMemory", "vkBindImageMemory", "vkGetBufferMemoryRequirements", "vkGetImageMemoryRequirements", "vkGetImageSparseMemoryRequirements", "vkGetPhysicalDeviceSparseImageFormatProperties",
		"vkQueueBindSparse", "vkCreateFence", "vkDestroyFence", "vkResetFences", "vkGetFenceStatus", "vkWaitForFences", "vkCreateSemaphore", "vkDestroySemaphore", "vkCreateEvent", "vkDestroyEvent",
		"vkGetEventStatus", "vkSetEvent", "vkResetEvent", "vkCreateQueryPool", "vkDestroyQueryPool", "vkGetQueryPoolResults", "vkCreateBuffer", "vkDestroyBuffer", "vkCreateBufferView", "vkDestroyBufferView",
		"vkCreateImage", "vkDestroyImage", "vkGetImageSubresourceLayout", "vkCreateImageView", "vkDestroyImageView", "vkCreateShaderModule", "vkDestroyShaderModule", "vkCreatePipelineCache",
		"vkDestroyPipelineCache", "vkGetPipelineCacheData", "vkMergePipelineCaches", "vkCreateGraphicsPipelines", "vkCreateComputePipelines", "vkDestroyPipeline", "vkCreatePipelineLayout",
		"vkDestroyPipelineLayout", "vkCreateSampler", "vkDestroySampler", "vkCreateDescriptorSetLayout", "vkDestroyDescriptorSetLayout", "vkCreateDescriptorPool", "vkDestroyDescriptorPool",
		"vkResetDescriptorPool", "vkAllocateDescriptorSets", "vkFreeDescriptorSets", "vkUpdateDescriptorSets", "vkCreateFramebuffer", "vkDestroyFramebuffer", "vkCreateRenderPass", "vkDestroyRenderPass",
		"vkGetRenderAreaGranularity", "vkCreateCommandPool", "vkDestroyCommandPool", "vkResetCommandPool", "vkAllocateCommandBuffers", "vkFreeCommandBuffers", "vkBeginCommandBuffer", "vkEndCommandBuffer",
		"vkResetCommandBuffer", "vkCmdBindPipeline", "vkCmdSetViewport", "vkCmdSetScissor", "vkCmdSetLineWidth", "vkCmdSetDepthBias", "vkCmdSetBlendConstants", "vkCmdSetDepthBounds",
		"vkCmdSetStencilCompareMask", "vkCmdSetStencilWriteMask", "vkCmdSetStencilReference", "vkCmdBindDescriptorSets", "vkCmdBindIndexBuffer", "vkCmdBindVertexBuffers", "vkCmdDraw", "vkCmdDrawIndexed",
		"vkCmdDrawIndirect", "vkCmdDrawIndexedIndirect", "vkCmdDispatch", "vkCmdDispatchIndirect", "vkCmdCopyBuffer", "vkCmdCopyImage", "vkCmdBlitImage", "vkCmdCopyBufferToImage", "vkCmdCopyImageToBuffer",
		"vkCmdUpdateBuffer", "vkCmdFillBuffer", "vkCmdClearColorImage", "vkCmdClearDepthStencilImage", "vkCmdClearAttachments", "vkCmdResolveImage", "vkCmdSetEvent", "vkCmdResetEvent", "vkCmdWaitEvents",
		"vkCmdPipelineBarrier", "vkCmdBeginQuery", "vkCmdEndQuery", "vkCmdResetQueryPool", "vkCmdWriteTimestamp", "vkCmdCopyQueryPoolResults", "vkCmdPushConstants", "vkCmdBeginRenderPass", "vkCmdNextSubpass",
		"vkCmdEndRenderPass", "vkCmdExecuteCommands"
	};

	size_t size = sizeof(methodsNames) / sizeof(methodsNames[0]);  // Calculate array size

	// Allocate memory properly
	MethodsTable = static_cast<uintptr_t*>(::calloc(size, sizeof(uintptr_t)));
	if (!MethodsTable) {
		Log("Failed to allocate memory for MethodsTable!");
		return;
	}

	// Get function addresses
	for (size_t i = 0; i < size; i++)
	{
		MethodsTable[i] = reinterpret_cast<uintptr_t>(::GetProcAddress(libVulkan, methodsNames[i]));
	}

	// Initialize MinHook
	MH_Initialize();

	// Create hooks for Vulkan draw calls
	CreateHook(16, reinterpret_cast<void**>(&oVkGetDeviceQueue), reinterpret_cast<void*>(hkVkGetDeviceQueue)); //get device (injector should inject as soon as the game starts, else device fails)
	CreateHook(106, reinterpret_cast<void**>(&oVkCmdDrawIndexed), reinterpret_cast<void*>(hkVkCmdDrawIndexed));
	CreateHook(105, reinterpret_cast<void**>(&oVkCmdDraw), reinterpret_cast<void*>(hkVkCmdDraw));
	CreateHook(107, reinterpret_cast<void**>(&oVkCmdDrawIndirect), reinterpret_cast<void*>(hkVkCmdDrawIndirect));
	CreateHook(108, reinterpret_cast<void**>(&oVkCmdDrawIndexedIndirect), reinterpret_cast<void*>(hkVkCmdDrawIndexedIndirect));
//	CreateHook(2, reinterpret_cast<void**>(&oVkEnumeratePhysicalDevices), reinterpret_cast<void*>(hkVkEnumeratePhysicalDevices));
//	CreateHook(64, reinterpret_cast<void**>(&original_vkCreateGraphicsPipelines), reinterpret_cast<void*>(Hooked_vkCreateGraphicsPipelines));
//	CreateHook(92, reinterpret_cast<void**>(&original_vkCmdBindPipeline), reinterpret_cast<void*>(Hooked_vkCmdBindPipeline));
//	CreateHook(132, reinterpret_cast<void**>(&oVkCmdBeginRenderPass), reinterpret_cast<void*>(hkVkCmdBeginRenderPass));
//	CreateHook(46, reinterpret_cast<void**>(&Original_vkCreateQueryPool), reinterpret_cast<void*>(Hooked_vkCreateQueryPool));
//	CreateHook(78, reinterpret_cast<void**>(&oVkUpdateDescriptorSets), reinterpret_cast<void*>(hkVkUpdateDescriptorSets));
//	CreateHook(131, reinterpret_cast<void**>(&oVkCmdPushConstants), reinterpret_cast<void*>(hkVkCmdPushConstants));
//	CreateHook(102, reinterpret_cast<void**>(&oVkCmdBindDescriptorSets), reinterpret_cast<void*>(hkVkCmdBindDescriptorSets));
//	CreateHook(89, reinterpret_cast<void**>(&oVkBeginCommandBuffer), reinterpret_cast<void*>(hvkBeginCommandBuffer));
//	CreateHook(17, reinterpret_cast<void**>(&oVkQueueSubmit), reinterpret_cast<void*>(hkVkQueueSubmit));
//	CreateHook(27, reinterpret_cast<void**>(&oVkBindBufferMemory), reinterpret_cast<void*>(hkVkBindBufferMemory));
//	CreateHook(64, reinterpret_cast<void**>(&original_vkCreateGraphicsPipelines), reinterpret_cast<void*>(Hooked_vkCreateGraphicsPipelines));
//	CreateHook(104, reinterpret_cast<void**>(&oVkCmdBindVertexBuffers), reinterpret_cast<void*>(hkVkCmdBindVertexBuffers));
}

//=======================================================================================//

DWORD WINAPI MainThread(LPVOID lpParam)
{
	HookVulkanFunctions();

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Log("1");
		DisableThreadLibraryCalls(hModule);
		CreateThread(0, 0, MainThread, hModule, 0, 0);
		break;

	case DLL_PROCESS_DETACH:
		DisableAll();
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) int NextHook(int code, WPARAM wParam, LPARAM lParam) { return CallNextHookEx(NULL, code, wParam, lParam); }