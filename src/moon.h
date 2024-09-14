#pragma once
#include <windows.h>

namespace Moon {

	enum class EShaderType {
		Vertex,
		Pixel
	};

	enum class EVertexDeclType {
		PositionColor
	};

	typedef void (*UpdateFunc)(float elapsed);
	typedef void* ShaderPtr;
	typedef void* MeshPtr;
	typedef void* TexturePtr;
	typedef void* MaterialPtr;

	bool CreateAppWindow(HINSTANCE hInstance, const char* title, int xres, int yres, bool bfullscreen, bool bconsoleouput, UpdateFunc func);
	void DestroyAppWindow();
	void Run();

	// Load/Prepare to GPU assets
	MaterialPtr CreateMaterial(const char* name, EVertexDeclType vertexType);
	void        CompileMaterial(MaterialPtr material);
	void        LoadShader(MaterialPtr material, const char* filename, EShaderType shaderType, const char* mainFuncName);
	TexturePtr  LoadTextureTGA(const char* filename);
	TexturePtr  LoadTextureTGA(MaterialPtr material, const char* filename, const char* sampleName);
	void        AddTexture(MaterialPtr material, TexturePtr texture, const char* sampleName);
	MeshPtr		  CreateMesh(const void* buffer, int size, int nvertices);

	// Draw functions
	void     SetClearColor(float r, float g, float b, float a);
	void     DrawMesh(MaterialPtr material, MeshPtr mesh);

};