﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <functional>
#include <deque>
#include <memory>
#include <vk_mesh.h>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL_events.h>
#include <frustum_cull.h>


namespace assets { struct PrefabInfo; }


//forward declarations
namespace vkutil {
	class DescriptorLayoutCache;
	class DescriptorAllocator;
}

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};



struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

	template<typename F>
    void push_function(F&& function) {
		static_assert(sizeof(F) < 200, "DONT CAPTURE TOO MUCH IN THE LAMBDA");
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); //call functors
        }

        deletors.clear();
    }
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};


struct Material {
	VkDescriptorSet textureSet{VK_NULL_HANDLE};
	VkPipeline pipeline;
	//VkPipelineLayout pipelineLayout;
	struct ShaderEffect* effect;
};

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
};



struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;

	RenderBounds bounds;
};


struct FrameData {
	VkSemaphore _presentSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _frameDeletionQueue;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	AllocatedBuffer objectBuffer;
	AllocatedBuffer instanceBuffer;
	AllocatedBuffer dynamicDataBuffer;
	vkutil::DescriptorAllocator* dynamicDescriptorAllocator;
};

struct UploadContext {
	VkFence _uploadFence;
	VkCommandPool _commandPool;	
};
struct GPUCameraData{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;

	
};


struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct PlayerCamera {
	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 inputAxis;

	

	float pitch{0}; //up-down rotation
	float yaw{0}; //left-right rotation

	glm::mat4 get_rotation_matrix();
};

struct EngineStats {
	float frametime;
	int objects;
	int drawcalls;
	int draws;
};


constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	int _selectedShader{ 0 };

	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;

	VkPhysicalDeviceProperties _gpuProperties;

	FrameData _frames[FRAME_OVERLAP];
	
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	
	VkRenderPass _renderPass;

	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swachainImageFormat;

	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;	

    DeletionQueue _mainDeletionQueue;
	
	VmaAllocator _allocator; //vma lib allocator

	//depth resources
	VkImageView _depthImageView;
	AllocatedImage _depthImage;

	//the format for the depth image
	VkFormat _depthFormat;

	
	vkutil::DescriptorAllocator* _descriptorAllocator;
	vkutil::DescriptorLayoutCache* _descriptorLayoutCache;

	VkDescriptorSetLayout _singleTextureSetLayout;

	GPUSceneData _sceneParameters;

	UploadContext _uploadContext;

	PlayerCamera _camera;

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();
	
	FrameData& get_current_frame();
	FrameData& get_last_frame();

	//default array of renderable objects
	std::vector<RenderObject> _renderables;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;
	std::unordered_map<std::string, Texture> _loadedTextures;
	std::unordered_map<std::string, assets::PrefabInfo*> _prefabCache;
	//functions

	//create material and add it to the map

	Material* create_material(VkPipeline pipeline, ShaderEffect* effect, const std::string& name);

	Material* clone_material(const std::string& originalname, const std::string& copyname);

	//returns nullptr if it cant be found
	Material* get_material(const std::string& name);

	//returns nullptr if it cant be found
	Mesh* get_mesh(const std::string& name);

	//our draw function
	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags required_flags = 0);

	size_t pad_uniform_buffer_size(size_t originalSize);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	bool load_prefab(const char* path, glm::mat4 root);

	std::string asset_path(const char* path);
	std::string asset_path(std::string& path);

	void refresh_renderbounds(RenderObject* object);
private:
	EngineStats stats;
	void process_input_event(SDL_Event* ev);
	void update_camera(float deltaSeconds);

	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

	void init_pipelines();

	void init_scene();

	void build_texture_set(VkSampler blockySampler, Material* texturedMat, const char* textureName);

	void init_descriptors();

	void init_imgui();

	

	void load_meshes();

	void load_images();

	bool load_image_to_cache(const char* name, const char* path);

	void upload_mesh(Mesh& mesh);
};
