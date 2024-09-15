#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>

namespace Moon {

	class MoonMesh;
	class MoonTexture;
	class MoonMaterial;
	class MoonShader;

	enum class EShaderType {
		Vertex,
		Pixel
	};

	enum class EVertexDeclType {
		PositionColor
	};

	typedef void (*UpdateFunc)(float elapsed);
	typedef void (*RenderFunc)();

	bool CreateAppWindow(HINSTANCE hInstance, const char* title, int xres, int yres, bool bfullscreen, bool bconsoleouput);
	void SetCallbacks(UpdateFunc updateFunc, RenderFunc renderFunc);
	void DestroyAppWindow();
	void Run();

	// Load/Prepare to GPU assets
	MoonMaterial* CreateMaterial(const char* name);
	MoonMesh*     CreateMesh(const void* buffer, int size, int nvertices);
	MoonTexture*  LoadTextureTGA(const char* filename);
	MoonShader*		LoadShader(const char* filename, EShaderType shaderType, const char *mainFuncName);

	// Material functions
	void SetMaterialVtxDecl(MoonMaterial* material, EVertexDeclType vertexType);
	void SetMaterialTexture(MoonMaterial* material, const char* sample, MoonTexture* texture);
	void SetMaterialShader(MoonMaterial* material, MoonShader*shader);
	//void ReloadMaterialShaders(MoonMaterial* material);
	void CompileMaterial(MoonMaterial* material);

	// Draw functions
	void SetClearColor(float r, float g, float b, float a);
	void DrawMesh(MoonMaterial* material, MoonMesh* mesh);

};