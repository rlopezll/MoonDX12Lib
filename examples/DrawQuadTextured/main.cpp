#include "moon.h"
#include <iostream>

Moon::MeshPtr mesh;
Moon::MaterialPtr material;

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
	Moon::DrawMesh(material, mesh);
}

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	Moon::CreateAppWindow(hInstance, "Hello World!", 1280, 720, false, true, &Update);
	
	// Test output console
	printf("Hello World\n");
	std::cout << "Hello World = " << 3 << std::endl;

	Moon::SetClearColor(102.0f / 255.f, 147.0f / 255.f, 245.0f / 255.f, 1.0f);

	mesh = Moon::CreateMesh(&triangleVertices[0], sizeof(triangleVertices), 6);
	material = Moon::CreateMaterial("basic", Moon::EVertexDeclType::PositionColor);
	Moon::LoadShader(material, "basic.hlsl", Moon::EShaderType::Vertex, "VSMain");
	Moon::LoadShader(material, "basic.hlsl", Moon::EShaderType::Pixel, "PSMain");
	////Moon::LoadTextureTGA(material, "wall_stone_albedo.tga", "albedo");
	Moon::CompileMaterial(material);

	Moon::Run();

	Moon::DestroyAppWindow();
	return 0;
}
