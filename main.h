//#define LOG_FILE_PATH "Log.txt"
#define LOG_FILE_PATH "C:\\Users\\User\\Desktop\\VulkanHook\\x64\\Release\\Log.txt"

void Log(const char* format, ...) {
	FILE* logFile;
	if (fopen_s(&logFile, LOG_FILE_PATH, "a") != 0) return;

	va_list args;
	va_start(args, format);
	vfprintf(logFile, format, args);
	va_end(args);

	fprintf(logFile, "\n");
	fclose(logFile);
}

bool waitedOnce = false;
void lognospam(int duration, const char* name)
{
	if (!waitedOnce)
	{
		int n;

		for (n = 0; n < duration; n++)
		{
			if (GetTickCount() % 100)
				Log(name);
		}
		waitedOnce = true;
	}
}

#ifdef _UNICODE
# define VTEXT(text) L##text
#else
# define VTEXT(text) text
#endif

// Vulkan method table
static uintptr_t* MethodsTable = nullptr;  // Use uintptr_t for safe pointer storage

bool CreateHook(uint16_t Index, void** Original, void* Function) {
	assert(Index >= 0 && Original != nullptr && Function != nullptr);

	void* target = reinterpret_cast<void*>(MethodsTable[Index]);  // Safe pointer cast

	if (target == nullptr) {
		return false;  // Prevent hooking a null pointer
	}

	if (MH_CreateHook(target, Function, Original) != MH_OK || MH_EnableHook(target) != MH_OK) {
		return false;
	}
	return true;
}

void DisableHook(uint16_t Index) {
	assert(Index >= 0);
	MH_DisableHook((void*)MethodsTable[Index]);
}

void DisableAll() {
	MH_DisableHook(MH_ALL_HOOKS);
	free(MethodsTable);
	MethodsTable = NULL;
}

void createCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = g_queueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(g_device, &poolInfo, nullptr, &g_commandPool) != VK_SUCCESS) {
		Log("failed to create command pool!");
	}
}

//cratepipeline



/*
Disabling ambient occlusion (AO) in Vulkan depends on how AO is implemented in the engine or application you're hooking. Here are some potential ways to disable AO in a Vulkan hook:
1. Hook the Ambient Occlusion Shader

Most modern engines use a shader to apply screen-space ambient occlusion (SSAO) or other AO techniques. You can try hooking into vkCreateShaderModule and detecting the AO shader by analyzing shader code or shader names.

	If you find the AO shader, modify or replace it with a pass-through shader that does nothing.

2. Modify Descriptor Sets (For SSAO Textures and Buffers)

Some engines use SSAO as a fullscreen pass where AO data is stored in a texture. If AO is sampled from a descriptor (e.g., VkDescriptorSet), you can try:

	Hook vkUpdateDescriptorSets and nullify the SSAO texture.
	Replace the AO texture with a blank texture (e.g., a fully white texture so AO has no effect).

Example of replacing AO texture:

VkDescriptorImageInfo blankAOTexture = {};
blankAOTexture.imageView = myWhiteTextureView;  // Use a preloaded 1x1 white texture
blankAOTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

VkWriteDescriptorSet writeAO = {};
writeAO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
writeAO.dstSet = aoDescriptorSet; // Replace with correct set
writeAO.dstBinding = aoBinding; // Find which binding is used for AO
writeAO.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
writeAO.descriptorCount = 1;
writeAO.pImageInfo = &blankAOTexture;

vkUpdateDescriptorSets(device, 1, &writeAO, 0, nullptr);

	You will need to find which descriptor set and binding correspond to SSAO.

3. Disable AO in the Render Pass (SSAO Pass Skip)

	Hook vkCmdBeginRenderPass and check if it's an SSAO pass.
	If you detect an AO render pass, skip rendering for that pass.

Example:

if (isSSAO(pass)) {
	return; // Skip AO render pass
}

	You can detect SSAO passes by looking at VkRenderPassBeginInfo::renderPass and checking if the pass has an AO-related format (e.g., R8 texture, used for AO occlusion).

4. Hook SSAO Compute Shaders (For Ray-Traced AO or Compute AO)

Some engines use compute shaders for AO (especially for ray-traced AO). If you detect an AO compute shader, you can:

	Hook vkCmdDispatch and block SSAO compute dispatches.
	Modify AO compute shader output buffers.

Which Method Should You Use?

	If AO is a fullscreen pass → Try method #2 (descriptor sets) or #3 (skipping render pass).
	If AO is done via shaders → Try #1 (hooking shader modules).
	If AO is computed via compute shaders → Try #4 (hooking compute dispatch).
*/