#include "moon.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <d3dx12.h> // Incluye la cabecera para CD3DX12
#include <dxgi1_6.h>
#include <map>
#include <stdexcept>
#include <vector>
#include <wrl.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <DirectXMath.h>

using namespace Microsoft::WRL;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Moon {
	
#define CheckFailedRet(func) { \
		HRESULT hr = func; \
		assert(SUCCEEDED(hr)); \
			if (FAILED(hr)) { \
					printf("DX12 Failed! Error code: %d\n", hr); \
					return; \
			}\
	}

#define CheckFailedRetValue(func, val) { \
		HRESULT hr = func; \
		assert(SUCCEEDED(hr)); \
			if (FAILED(hr)) { \
					printf("DX12 Failed! Error code: %d\n", hr); \
					return val; \
			}\
	}

#define CheckMoonContext(ctx) { \
		assert(ctx); \
			if (!ctx) { \
					printf("Moon Framework is not initialized!\n"); \
					return; \
			}\
	}

#define CheckMoonContextRetBool(ctx) { \
		assert(ctx); \
			if (!ctx) { \
					printf("Moon Framework is not initialized!\n"); \
					return false; \
			}\
	}

#define CheckMoonContextRetValue(ctx, failed) { \
		assert(ctx); \
			if (!ctx) { \
					printf("Moon Framework is not initialized!\n"); \
					return failed; \
			}\
	}

	struct MoonContext {
		static const int nFrameCount = 2;
		HWND hWnd = nullptr;
		int width = 800;
		int height = 600;
		UpdateFunc func = nullptr;
		bool bConsoleOuput = false;
		float clearColor[4];

		// Dx12 vars
		ComPtr<ID3D12Device>              device;
		ComPtr<IDXGISwapChain4>           swapChain;
		ComPtr<ID3D12Resource>            renderTargets[nFrameCount];
		ComPtr<ID3D12CommandAllocator>    commandAllocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12CommandQueue>				commandQueue;
		ComPtr<ID3D12RootSignature>				rootSignature; //tmp
		ComPtr<ID3D12DescriptorHeap>			rtvHeap;
		ComPtr<ID3D12DescriptorHeap>			srvHeap;
		ComPtr<ID3D12Fence>								fence;
		HANDLE														fenceEvent;
		CD3DX12_VIEWPORT viewport;
		CD3DX12_RECT scissorRect;
		UINT rtvDescriptorSize = 0;
		UINT frameIndex = 0;
		UINT fenceValue = 1;
		bool vSync = false;
		bool bFullscreen = false;
		bool bFirstUpdate = true;
		RECT windowRect;

		std::map<std::string, ComPtr<ID3DBlob>> shaders;
	};

	struct MoonMesh {
		ComPtr<ID3D12Resource>	 vertexBuffer;
		int                      nvertices = 0;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	};

	struct MoonTexture {
		ComPtr<ID3D12Resource>	 texture;
		void*										 data;
		int                      width;
		int                      height;
		int                      bpp;
	};

	struct MoonMaterial {
		std::string name;
		EVertexDeclType vertexType;
		std::map<std::string, MoonTexture*> textures; //sampler, texture
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;
		ComPtr<ID3D12PipelineState> pipelineState;
	};

	// Forward declarations
	LRESULT CALLBACK WndProc(HWND hwnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam);

	void Update();
	void Resize(uint32_t width, uint32_t height);
	void LoadPipeline();

	ComPtr<IDXGIAdapter4> GetAdapter();
	D3D12_INPUT_ELEMENT_DESC* GetInputElementDesc(EVertexDeclType vertexType, int* nInputElements);
	void WaitForPreviousFrame();
	void SetFullscreen(bool bFullscreen);


	// Global vars
	MoonContext* Context = nullptr;

	bool CreateAppWindow(HINSTANCE hInstance, const char* title, int xres, int yres, bool bfullscreen, bool bconsoleouput, UpdateFunc func)
	{
		assert(!Context);
		if (Context) {
			printf("Moon Framework already initialized!\n");
			return false;
		}

		Context = new MoonContext();
		assert(Context);
		if (!Context) {
			printf("Moon Framework can't be created!\n");
			return false;
		}

		// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
		// Using this awareness context allows the client area of the window 
		// to achieve 100% scaling while still allowing non-client window content to 
		// be rendered in a DPI sensitive fashion.
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		Context->func = func;
		Context->width = xres;
		Context->height = yres;
		Context->bConsoleOuput = bconsoleouput;

		// Crear ventana
		WNDCLASSEX wc = {};
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hInstance;
		wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wc.lpszClassName = "DX12WindowClass";

		static HRESULT hr = RegisterClassEx(&wc);
		assert(SUCCEEDED(hr));

		int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

		RECT windowRect = { 0, 0, static_cast<LONG>(xres), static_cast<LONG>(yres) };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		int windowWidth = windowRect.right - windowRect.left;
		int windowHeight = windowRect.bottom - windowRect.top;

		// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
		int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
		int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

		Context->hWnd = CreateWindow(
			wc.lpszClassName, 
			title,
			WS_OVERLAPPEDWINDOW, 
			windowX, 
			windowY,
			xres, 
			yres, 
			nullptr, // We have no parent window.
			nullptr, // We aren't using menus.
			hInstance, 
			nullptr);

		assert(Context->hWnd);

		LoadPipeline();

		if (bfullscreen) {
			SetFullscreen(true);
		}
		else {
			ShowWindow(Context->hWnd, SW_SHOWDEFAULT);
		}

		// create the console
		if (Context->bConsoleOuput) {
			if (AllocConsole()) {
				FILE* pCout;
				freopen_s(&pCout, "CONOUT$", "w", stdout);
				SetConsoleTitle("Debug Console");
				SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
			}
		}

		return true;
	}

	void DestroyAppWindow()
	{
		CheckMoonContext(Context);

#if defined(_DEBUG)
		ComPtr<IDXGIDebug1> debugController;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugController))))
		{
			debugController->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
#endif

		WaitForPreviousFrame();
		CloseHandle(Context->fenceEvent);

		if (Context->bConsoleOuput) {
			FreeConsole();
		}

		delete Context;
		Context = nullptr;

	}

	void Run()
	{
		CheckMoonContext(Context);

		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	void SetClearColor(float r, float g, float b, float a)
	{
		CheckMoonContext(Context);
		Context->clearColor[0] = r;
		Context->clearColor[1] = g;
		Context->clearColor[2] = b;
		Context->clearColor[3] = a;
	}

	void DrawMesh(MaterialPtr rawMaterial, MeshPtr rawMesh)
	{
		CheckMoonContext(Context);
		if(!rawMaterial || !rawMesh)
			return;

		MoonMaterial* material = static_cast<MoonMaterial*>(rawMaterial);
		MoonMesh* mesh = static_cast<MoonMesh*>(rawMesh);
		assert(material->pipelineState); // is material compiled?!

		Context->commandList->SetPipelineState(material->pipelineState.Get());
		Context->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		Context->commandList->IASetVertexBuffers(0, 1, &mesh->vertexBufferView);
		Context->commandList->DrawInstanced(mesh->nvertices, 1, 0, 0);
	}

	void LoadShader(MaterialPtr rawMaterial, const char* filename, EShaderType shaderType, const char* mainFuncName)
	{
		if(!rawMaterial)
		 return;

		MoonMaterial* material = static_cast<MoonMaterial *>(rawMaterial);

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		LPCSTR shaderTypeStr = "vs_5_0";
		if (shaderType == EShaderType::Pixel) {
			shaderTypeStr = "ps_5_0";
		}

		int strSize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
		wchar_t* wfilename = new wchar_t[strSize];
		MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, strSize);

		strSize = MultiByteToWideChar(CP_UTF8, 0, mainFuncName, -1, nullptr, 0);
		wchar_t* wmainFuncName = new wchar_t[strSize];
		MultiByteToWideChar(CP_UTF8, 0, mainFuncName, -1, wmainFuncName, strSize);

		ID3DBlob* shader = nullptr;
		HRESULT hr = D3DCompileFromFile(wfilename, nullptr, nullptr, mainFuncName, shaderTypeStr, compileFlags, 0, &shader, nullptr);
		assert(SUCCEEDED(hr));

		if (shaderType == EShaderType::Vertex) {
			material->vertexShader = shader;
		}	
		else if (shaderType == EShaderType::Pixel) {
			material->pixelShader = shader;
		}
		delete[] wfilename;
	}

	MeshPtr CreateMesh(const void* buffer, int size, int nvertices)
	{
		MoonMesh* mesh = new MoonMesh();

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC resourceDescBuffer = CD3DX12_RESOURCE_DESC::Buffer(size);
		HRESULT hr = Context->device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDescBuffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mesh->vertexBuffer));

		assert(SUCCEEDED(hr));
		if (FAILED(hr)) {
			delete[] mesh;
			return nullptr;
		}

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		hr = mesh->vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		assert(SUCCEEDED(hr));
		if (FAILED(hr)) {
			delete[] mesh;
			return nullptr;
		}

		memcpy(pVertexDataBegin, buffer, size);
		mesh->vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		mesh->vertexBufferView.BufferLocation = mesh->vertexBuffer->GetGPUVirtualAddress();
		mesh->vertexBufferView.StrideInBytes = size / nvertices;
		mesh->vertexBufferView.SizeInBytes = size;
		mesh->nvertices = nvertices;
		return mesh;
	}


	MaterialPtr CreateMaterial(const char* name, EVertexDeclType vertexType)
	{
		CheckMoonContextRetValue(Context, nullptr);

		MoonMaterial* material = new MoonMaterial();
		material->name = name;
		material->vertexType = vertexType;

		return material;
	}

	void CompileMaterial(MaterialPtr rawMaterial)
	{
		if(!rawMaterial)
			return;

		CheckMoonContext(Context);
		MoonMaterial* material = static_cast<MoonMaterial*>(rawMaterial);

		int nInputElements = 0;
		D3D12_INPUT_ELEMENT_DESC* inputElementsDescs = GetInputElementDesc(material->vertexType, &nInputElements);
		assert(inputElementsDescs);

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementsDescs, (UINT)nInputElements };
		psoDesc.pRootSignature = Context->rootSignature.Get();
		if (material->vertexShader) {
			psoDesc.VS = CD3DX12_SHADER_BYTECODE(material->vertexShader.Get());
		}
		if (material->pixelShader) {
			psoDesc.PS = CD3DX12_SHADER_BYTECODE(material->pixelShader.Get());
		}
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		CheckFailedRet(Context->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&material->pipelineState)));

		// Create the command list.
		//CheckFailedRet(Context->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Context->commandAllocator.Get(), material->pipelineState.Get(), IID_PPV_ARGS(&Context->commandList)));
		//Context->commandList->SetName(L"Moon main commandlist");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		//Context->commandList->Close();
	}

#pragma pack(push, 1)
	struct TGAHeader {
		uint8_t id_length;
		uint8_t color_map_type;
		uint8_t image_type;
		uint16_t color_map_start;
		uint16_t color_map_length;
		uint8_t color_map_bits;
		uint16_t x_origin;
		uint16_t y_origin;
		uint16_t width;
		uint16_t height;
		uint8_t bits_per_pixel;
		uint8_t image_descriptor;
	};
#pragma pack(pop)

	TexturePtr LoadTextureTGA(const char* filename)
	{
		FILE* file;
		errno_t err;
		err = fopen_s(&file, filename, "rb");
		assert(err == 0);
		if (err != 0) {
			printf("Unable to open file");
			return nullptr;
		}

		TGAHeader header;
		fread(&header, sizeof(TGAHeader), 1, file);

		assert(header.image_type == 2 || header.image_type == 10);
		if (header.image_type != 2 && header.image_type != 10) {
			printf("Unsupported TGA format\n");
			fclose(file);
			return nullptr;
		}

		int width = header.width;
		int height = header.height;
		int bpp = header.bits_per_pixel / 8;

		uint8_t* data = (uint8_t*)new uint8_t[width * height * bpp];
		if (!data) {
			printf("Unable to allocate memory");
			fclose(file);
			return nullptr;
		}

		fread(data, width * height * bpp, 1, file);

		MoonTexture* texture = new MoonTexture();
		assert(texture);
		if (!texture) {
			delete[] data;
			fclose(file);
			return nullptr;
		}
		texture->data = data;
		texture->width = width;
		texture->height = height;
		texture->bpp = header.bits_per_pixel;

		//printf("TGA Image: Width = %d, Height = %d, BPP = %d\n", width, height, bpp);
		// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
		// the command list that references it has finished executing on the GPU.
		// We will flush the GPU at the end of this method to ensure the resource is not
		// prematurely destroyed.
		ComPtr<ID3D12Resource> textureUploadHeap;

		// Create the texture.
		{
			// Describe and create a Texture2D.
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = width;
			textureDesc.Height = height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
			HRESULT hr = Context->device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&texture->texture));
			assert(SUCCEEDED(hr));
			if (FAILED(hr)) {
				delete texture;
				delete[] data;
				fclose(file);
				return nullptr;
			}

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture->texture.Get(), 0, 1);

			// Create the GPU upload buffer.
			const CD3DX12_HEAP_PROPERTIES heapProperties2(D3D12_HEAP_TYPE_UPLOAD);
			const CD3DX12_RESOURCE_DESC resourceDescBuffer2 = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
			hr = Context->device->CreateCommittedResource(
				&heapProperties2,
				D3D12_HEAP_FLAG_NONE,
				&resourceDescBuffer2,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap));
			if (FAILED(hr)) {
				delete texture;
				delete[] data;
				fclose(file);
				return nullptr;
			}

			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the Texture2D.

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = texture->data;
			textureData.RowPitch = width * bpp;
			textureData.SlicePitch = textureData.RowPitch * height;

			UpdateSubresources(Context->commandList.Get(), texture->texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
			const CD3DX12_RESOURCE_BARRIER barrier =CD3DX12_RESOURCE_BARRIER::Transition(texture->texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			Context->commandList->ResourceBarrier(1, &barrier);

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			Context->device->CreateShaderResourceView(texture->texture.Get(), &srvDesc, Context->srvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		fclose(file);
		return texture;
	}

	Moon::TexturePtr LoadTextureTGA(MaterialPtr rawMaterial, const char* filename, const char* sampleName)
	{
		if(!rawMaterial || !sampleName || strlen(sampleName) == 0)
			return nullptr;

		TexturePtr rawTexture = LoadTextureTGA(filename);
		AddTexture(rawMaterial,  rawTexture, sampleName);
		return rawTexture;
	}

	void AddTexture(MaterialPtr rawMaterial, TexturePtr rawTexture, const char* sampleName)
	{
		if (!rawMaterial || !sampleName || strlen(sampleName) == 0)
			return;

		MoonMaterial* material = static_cast<MoonMaterial*>(rawMaterial);
		MoonTexture* texture = static_cast<MoonTexture*>(rawTexture);
		material->textures[sampleName] = texture;
	}

	// Internal functions

	LRESULT CALLBACK WndProc(HWND hwnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam)
	{
		CheckMoonContextRetValue(Context, 0);
		if(Context->device) {
			switch (msg)
			{
			case WM_PAINT:
				Update();
				return 0;
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
			{
				bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

				switch (wParam)
				{
				case VK_RETURN:
					if (alt)
					{
				case VK_F11:
					SetFullscreen(!Context->bFullscreen);
					}
					break;
				}
			}
			return 0;
			// The default window procedure will play a system notification sound 
			// when pressing the Alt+Enter keyboard combination if this message is 
			// not handled.
			case WM_SYSCHAR:
				return 0;
			case WM_SIZE:
			{
				RECT clientRect = {};
				::GetClientRect(hwnd, &clientRect);

				int width = clientRect.right - clientRect.left;
				int height = clientRect.bottom - clientRect.top;

				Resize(width, height);
				return 0;

			}
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			}
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Update()
	{
		static double elapsedSeconds = 0.0;
		static std::chrono::high_resolution_clock clock;
		static auto t0 = clock.now();

		auto t1 = clock.now();
		auto deltaTime = t1 - t0;
		t0 = t1;
		elapsedSeconds += deltaTime.count() * 1e-9;

		CheckMoonContext(Context);
		if(Context->device == nullptr)
			return;

		if (Context->bFirstUpdate) {
			Context->bFirstUpdate = false;
			WaitForPreviousFrame();
		}

		// Command list allocators can only be reset when the associated 
		// command lists have finished execution on the GPU; apps should use 
		// fences to determine GPU execution progress.
		Context->commandAllocator->Reset();

		// However, when ExecuteCommandList() is called on a particular command 
		// list, that command list can then be reset at any time and must be before 
		// re-recording.
		Context->commandList->Reset(Context->commandAllocator.Get(), nullptr);

		// Set necessary state.
		Context->commandList->SetGraphicsRootSignature(Context->rootSignature.Get());
		Context->commandList->RSSetViewports(1, &Context->viewport);
		Context->commandList->RSSetScissorRects(1, &Context->scissorRect);

		// Indicate that the back buffer will be used as a render target.
		CD3DX12_RESOURCE_BARRIER preBarrier = CD3DX12_RESOURCE_BARRIER::Transition(Context->renderTargets[Context->frameIndex].Get(), 
																					D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context->commandList->ResourceBarrier(1, &preBarrier);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(Context->rtvHeap->GetCPUDescriptorHandleForHeapStart(), Context->frameIndex, Context->rtvDescriptorSize);
		Context->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		Context->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// Call external update
		if (Context->func) {
			Context->func((float)elapsedSeconds);
		}

		// Indicate that the back buffer will now be used to present.
		CD3DX12_RESOURCE_BARRIER postBarrier = CD3DX12_RESOURCE_BARRIER::Transition(Context->renderTargets[Context->frameIndex].Get(), 
																					 D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		Context->commandList->ResourceBarrier(1, &postBarrier);
		Context->commandList->Close();

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { Context->commandList.Get() };
		Context->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		// Present the frame.
		CheckFailedRet(Context->swapChain->Present(1, 0));

		WaitForPreviousFrame();
	}

	void Resize(uint32_t width, uint32_t height)
	{
		CheckMoonContext(Context);

		if (Context->width != width || Context->height != height)
		{
			// Don't allow 0 size swap chain back buffers.
			Context->width = width > 0 ? width : 1;
			Context->height = height > 0 ? height : 1;

			// Render update size 
		}
	}

	void LoadPipeline()
	{
		CheckMoonContext(Context);
		
		UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
		// Enable the debug layer (requires the Graphics Tools "optional feature").
		// NOTE: Enabling the debug layer after device creation will invalidate the active device.
		{
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// Enable additional debug layers.
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif

		ComPtr<IDXGIFactory4> dxgiFactory4;
		CheckFailedRet(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

		ComPtr<IDXGIAdapter4> adapter = GetAdapter();
		assert(adapter);
		if (!adapter) {
			printf("Error getting DirectX adapter!.");
			return;
		}

		CheckFailedRet(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&Context->device)));

		// Enable debug messages in debug mode.
#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(Context->device.As(&pInfoQueue)))
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// Suppress whole categories of messages
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
					D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] = {
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
					D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
					D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
			};

			D3D12_INFO_QUEUE_FILTER NewFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			NewFilter.DenyList.NumSeverities = _countof(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = _countof(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			CheckFailedRet(pInfoQueue->PushStorageFilter(&NewFilter));
		}
#endif

		// Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		CheckFailedRet(Context->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&Context->commandQueue)));

		BOOL allowTearing = FALSE;
		ComPtr<IDXGIFactory5> dxgiFactory5;
		if (SUCCEEDED(dxgiFactory4.As(&dxgiFactory5)))
		{
			if (FAILED(dxgiFactory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}

		Context->viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Context->width), static_cast<float>(Context->height));
		Context->scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(Context->width), static_cast<LONG>(Context->height));

		// Describe and create the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = MoonContext::nFrameCount;
		swapChainDesc.Width = Context->width;
		swapChainDesc.Height = Context->height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc = { 1 ,0 };
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapChainDesc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		ComPtr<IDXGISwapChain1> swapChain;
		CheckFailedRet(dxgiFactory4->CreateSwapChainForHwnd(
			Context->commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
			Context->hWnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));
		// This sample does not support fullscreen transitions.
		CheckFailedRet(dxgiFactory4->MakeWindowAssociation(Context->hWnd, DXGI_MWA_NO_ALT_ENTER));

		CheckFailedRet(swapChain.As(&Context->swapChain));
		Context->frameIndex = Context->swapChain->GetCurrentBackBufferIndex();

		// Create descriptor heaps.
		{
			// Describe and create a render target view (RTV) descriptor heap.
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = MoonContext::nFrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			CheckFailedRet(Context->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&Context->rtvHeap)));
			Context->rtvDescriptorSize = Context->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// Describe and create a shader resource view (SRV) heap for the texture.
			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.NumDescriptors = 1;
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			CheckFailedRet(Context->device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&Context->srvHeap)));
		}

		// Create frame resources.
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(Context->rtvHeap->GetCPUDescriptorHandleForHeapStart());

			// Create a RTV for each frame.
			for (UINT n = 0; n < MoonContext::nFrameCount; n++)
			{
				CheckFailedRet(Context->swapChain->GetBuffer(n, IID_PPV_ARGS(&Context->renderTargets[n])));
				Context->device->CreateRenderTargetView(Context->renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(Context->rtvDescriptorSize);
			}
		}

		CheckFailedRet(Context->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Context->commandAllocator)));

		//Create Fence
		CheckFailedRet(Context->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Context->fence)));
		Context->fenceValue = 1;
		Context->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		assert(Context->fenceEvent); //HRESULT_FROM_WIN32(GetLastError()) to check the error


		// Create the root signature.
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			CheckFailedRet(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			CheckFailedRet(Context->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&Context->rootSignature)));
			Context->rootSignature->SetName(L"Moon root signature");
		}

		// Create the command list.
		CheckFailedRet(Context->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Context->commandAllocator.Get(), nullptr, IID_PPV_ARGS(&Context->commandList)));
		Context->commandList->SetName(L"Moon main commandlist");

		// Command lists are created in the recording state, but there is nothing
		// to record yet. The main loop expects it to be closed, so close it now.
		Context->commandList->Close();

	}

	ComPtr<IDXGIAdapter4> GetAdapter() {
		ComPtr<IDXGIFactory4> factory;
		CreateDXGIFactory1(IID_PPV_ARGS(&factory));

		ComPtr<IDXGIAdapter1> dxgiAdapter1;
		ComPtr<IDXGIAdapter4> dxgiAdapter4;

		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; factory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			// Check to see if the adapter can create a D3D12 device without actually 
			// creating it. The adapter with the largest dedicated video memory
			// is favored.
			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				HRESULT hr = dxgiAdapter1.As(&dxgiAdapter4);
				assert(SUCCEEDED(hr));
			}
		}

		return dxgiAdapter4;
	}

	D3D12_INPUT_ELEMENT_DESC* GetInputElementDesc(EVertexDeclType vertexType, int* nInputElements) {
		D3D12_INPUT_ELEMENT_DESC* inputElementDescs = nullptr;
		if (vertexType == EVertexDeclType::PositionColor) {
			inputElementDescs = new D3D12_INPUT_ELEMENT_DESC[2];
			inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
			inputElementDescs[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
			*nInputElements = 2;
		}
		return inputElementDescs;
	}

	void WaitForPreviousFrame()
	{
		// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
		// This is code implemented as such for simplicity. More advanced samples 
		// illustrate how to use fences for efficient resource usage.

		// Wait for the GPU to be done with all resources.
		const uint64_t fenceValueForSignal = Context->fenceValue;
		HRESULT hr = Context->commandQueue->Signal(Context->fence.Get(), fenceValueForSignal);
		assert(SUCCEEDED(hr));
		++Context->fenceValue;

		// Wait until the previous frame is finished.
		if (Context->fence->GetCompletedValue() < fenceValueForSignal)
		{
			hr = Context->fence->SetEventOnCompletion(fenceValueForSignal, Context->fenceEvent);
			assert(SUCCEEDED(hr));
			WaitForSingleObject(Context->fenceEvent, INFINITE);
		}
		Context->frameIndex = Context->swapChain->GetCurrentBackBufferIndex();
	}

	void SetFullscreen(bool fullscreen)
	{
		CheckMoonContext(Context);

		if (Context->bFullscreen != fullscreen)
		{
			Context->bFullscreen = fullscreen;

			if (Context->bFullscreen) // Switching to fullscreen.
			{
				// Store the current window dimensions so they can be restored 
				// when switching out of fullscreen state.
				::GetWindowRect(Context->hWnd, &Context->windowRect);
				// Set the window style to a borderless window so the client area fills
						// the entire screen.
				UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

				::SetWindowLongW(Context->hWnd, GWL_STYLE, windowStyle);

				// Query the name of the nearest display device for the window.
						// This is required to set the fullscreen dimensions of the window
						// when using a multi-monitor setup.
				HMONITOR hMonitor = ::MonitorFromWindow(Context->hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFOEX monitorInfo = {};
				monitorInfo.cbSize = sizeof(MONITORINFOEX);
				::GetMonitorInfo(hMonitor, &monitorInfo);
				::SetWindowPos(Context->hWnd, HWND_TOP,
					monitorInfo.rcMonitor.left,
					monitorInfo.rcMonitor.top,
					monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
					monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);

				::ShowWindow(Context->hWnd, SW_MAXIMIZE);
			}
			else
			{
				// Restore all the window decorators.
				::SetWindowLong(Context->hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

				::SetWindowPos(Context->hWnd, HWND_NOTOPMOST,
					Context->windowRect.left,
					Context->windowRect.top,
					Context->windowRect.right - Context->windowRect.left,
					Context->windowRect.bottom - Context->windowRect.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);

				::ShowWindow(Context->hWnd, SW_NORMAL);
			}
		}
	}
}

