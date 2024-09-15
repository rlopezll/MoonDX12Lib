#include "moon.h"
#include <iostream>
#include <cassert>

Moon::MoonMesh *mesh = nullptr;
Moon::MoonMaterial *material = nullptr;

const float aspectRatio = 1280.0f / 720.0f;
float triangleVertices[] =
{
		 -0.25f, 0.25f * aspectRatio, 0.0f , 1.0f, 1.0f, 1.0f, 1.0f,
		 0.25f, -0.25f * aspectRatio, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		 -0.25f, -0.25f * aspectRatio, 0.0, 1.0f, 1.0f, 1.0f, 1.0f,
		 -0.25f, 0.25f * aspectRatio, 0.0f , 1.0f, 1.0f, 1.0f, 1.0f,
		 0.25f, 0.25f * aspectRatio, 0.0f , 1.0f, 1.0f, 1.0f, 1.0f,
		 0.25f, -0.25f * aspectRatio, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

void Update(float elapsed) {
}

void Render() {
	assert(material && mesh);
	Moon::DrawMesh(material, mesh);
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	Moon::CreateAppWindow(hInstance, "Hello World!", 1280, 720, false, true);
	Moon::SetCallbacks(&Update, &Render);
	
	// Test output console
	printf("Hello World\n");
	std::cout << "Hello World = " << 3 << std::endl;

	Moon::SetClearColor(102.0f / 255.f, 147.0f / 255.f, 245.0f / 255.f, 1.0f);

	Moon::MoonShader* vertexShader = Moon::LoadShader("basic.hlsl", Moon::EShaderType::Vertex, "VSMain");
	Moon::MoonShader* pixelShader = Moon::LoadShader("basic.hlsl", Moon::EShaderType::Pixel, "PSMain");

	material = Moon::CreateMaterial("basic");
	Moon::SetMaterialVtxDecl(material, Moon::EVertexDeclType::PositionColor);
	Moon::SetMaterialShader(material, vertexShader);
	Moon::SetMaterialShader(material, pixelShader);
	Moon::CompileMaterial(material);

	mesh = Moon::CreateMesh(&triangleVertices[0], sizeof(triangleVertices), 6);
	////Moon::LoadTextureTGA(material, "wall_stone_albedo.tga", "albedo");

	Moon::Run();

	Moon::DestroyAppWindow();
	return 0;
}
