#include "stdafx.h"
#include <stdio.h>

using namespace DirectX; // we will be using the directxmath library

/*struct Vertex {
	XMFLOAT3 pos;
};
*/
struct Vertex {
	Vertex(float x, float y, float z, float r, float g, float b, float a) : pos(x, y, z), color(r, g, b, z) {}
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

#define D3D12_CHECK(result) \
if(result != S_OK) \
{          \
printf("Directx12 Error: %s%d", __FILE__, __LINE__); \
__debugbreak();								\
return false;};				\

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

bool InitD3D(directxInfo* dInfo) {

	HRESULT hr;
	IDXGIFactory4* dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr)) {
		return false;
	};

	IDXGIAdapter1* adapter;
	int adapterIndex = 0;
	bool adapterFound = false;
	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 adapterDesc;
		adapter->GetDesc1(&adapterDesc);
		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {

			adapterIndex++;
			continue;
		}
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr)) {

			adapterFound = true;
			break;
		}
		adapterIndex++;
	}
	if (!adapterFound)
	{
		return false;
	};

	D3D12_CHECK(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dInfo->device)));

	D3D12_COMMAND_QUEUE_DESC cqDesc = {};
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	D3D12_CHECK(dInfo->device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&dInfo->commandQueue)));

	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = Width;
	backBufferDesc.Height = Height;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameBufferCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Windowed = !FullScreen;

	IDXGISwapChain* tempSwapChain;

	D3D12_CHECK(dxgiFactory->CreateSwapChain(dInfo->commandQueue, &swapChainDesc, &tempSwapChain));

	dInfo->swapChain = (IDXGISwapChain3*)tempSwapChain;
	dInfo->frameIndex = dInfo->swapChain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = frameBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	D3D12_CHECK(dInfo->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&dInfo->rtvDescriptorHeap)));

	dInfo->rtvDescriptorSize = dInfo->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(dInfo->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < frameBufferCount; i++) {
		D3D12_CHECK(dInfo->swapChain->GetBuffer(i, IID_PPV_ARGS(&dInfo->renderTargets[i])));
		dInfo->device->CreateRenderTargetView(dInfo->renderTargets[i], nullptr, rtvHandle);
		rtvHandle.Offset(1, dInfo->rtvDescriptorSize);
	};

	for (int i = 0; i < frameBufferCount; i++) {

		D3D12_CHECK(dInfo->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dInfo->commandAllocator[i])));

	}

	D3D12_CHECK(dInfo->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dInfo->commandAllocator[0], NULL, IID_PPV_ARGS(&dInfo->commandList)));
	//D3D12_CHECK(dInfo->commandList->Close());

	for (int i = 0; i < frameBufferCount; i++) {

		D3D12_CHECK(dInfo->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dInfo->fence[i])));
		dInfo->fenceValue[i] = 0;
	}

	dInfo->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); //event name?

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3DBlob* signature;
	D3D12_CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));

	D3D12_CHECK(dInfo->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&dInfo->rootSignature)));

	ID3DBlob* vertexShader;
	ID3DBlob* errorBuf;

	//D3D12_CHECK(D3DCompileFromFile(L"VertexShader.hlsl.cso", nullptr, nullptr, "main", "vs_5_0", //D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &errorBuf));
	HRESULT shaderHr;
	shaderHr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &errorBuf);
	if (FAILED(shaderHr)) {
		OutputDebugStringA((char*)errorBuf->GetBufferPointer());
		return false;
	}

	D3D12_SHADER_BYTECODE vertexShaderByteCode = {};
	vertexShaderByteCode.BytecodeLength = vertexShader->GetBufferSize();
	vertexShaderByteCode.pShaderBytecode = vertexShader->GetBufferPointer();

	ID3DBlob* pixelShader;
	shaderHr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &errorBuf);
	if (FAILED(shaderHr)) {
		OutputDebugStringA((char*)errorBuf->GetBufferPointer());
		return false;
	};



	D3D12_SHADER_BYTECODE pixelShaderByteCode = {};
	pixelShaderByteCode.BytecodeLength = pixelShader->GetBufferSize();
	pixelShaderByteCode.pShaderBytecode = pixelShader->GetBufferPointer();

	/*D3D12_INPUT_ELEMENT_DESC inputLayout[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
	*/
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = dInfo->rootSignature;
	psoDesc.VS = vertexShaderByteCode;
	psoDesc.PS = pixelShaderByteCode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc = sampleDesc;
	//psoDesc.NodeMask = 0;
	//psoDesc.CachedPSO = psoDesc; todo: figure out cached pipeeline (in vulkan as well)
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.NumRenderTargets = 1;

	D3D12_CHECK(dInfo->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dInfo->pipelineStateObject)));



	/*Vertex vList[] = {
		{ { 0.0f, 0.5f, 0.5f } },
		{ { 0.5f, -0.5f, 0.5f } },
		{ { -0.5f, -0.5f, 0.5f } },
	};
	

	Vertex vList[] = {
		{ 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{ 0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
		{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	};
	*/

	Vertex vList[] = {
	{ -0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	{  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
	{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	{  0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f }
	};

	int vBufferSize = sizeof(vList);

	// create default heap
	// default heap is memory on the GPU. Only the GPU has access to this memory
	// To get data into this heap, we will have to upload the data using
	// an upload heap
	dInfo->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
										// from the upload heap to this heap
		nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&dInfo->vertexBuffer));

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	dInfo->vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	// create upload heap
	// upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
	// We will upload the vertex buffer using this heap to the default heap
	ID3D12Resource* vBufferUploadHeap;
	dInfo->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vBufferUploadHeap));
	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(vList); // pointer to our vertex array
	vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
	vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

	// we are now creating a command with the command list to copy the data from
	// the upload heap to the default heap
	UpdateSubresources(dInfo->commandList, dInfo->vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	dInfo->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dInfo->vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// Create index buffer


















// a quad (2 triangles)
	DWORD iList[] = {
		0, 1, 2, // first triangle
		0, 3, 1 // second triangle
	};

	int iBufferSize = sizeof(iList);

	// create default heap to hold index buffer
	dInfo->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
		nullptr, // optimized clear value must be null for this type of resource
		IID_PPV_ARGS(&dInfo->indexBuffer));

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	dInfo->vertexBuffer->SetName(L"Index Buffer Resource Heap");

	// create upload heap to upload index buffer
	ID3D12Resource* iBufferUploadHeap;
	dInfo->device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&iBufferUploadHeap));
	vBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = (BYTE*)iList; // pointer to our index array
	indexData.RowPitch = iBufferSize; // size of all our index buffer
	indexData.SlicePitch = iBufferSize; // also the size of our index buffer

	// we are now creating a command with the command list to copy the data from
	// the upload heap to the default heap
	UpdateSubresources(dInfo->commandList, dInfo->indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	dInfo->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dInfo->indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
	dInfo->indexBufferView.BufferLocation = dInfo->indexBuffer->GetGPUVirtualAddress();
	dInfo->indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
	dInfo->indexBufferView.SizeInBytes = iBufferSize;






















	// Now we execute the command list to upload the initial assets (triangle data)
	dInfo->commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { dInfo->commandList };
	dInfo->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
	dInfo->fenceValue[dInfo->frameIndex]++;
	hr = dInfo->commandQueue->Signal(dInfo->fence[dInfo->frameIndex], dInfo->fenceValue[dInfo->frameIndex]);
	if (FAILED(hr))
	{
		Running = false;
	}

	// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
	dInfo->vertexBufferView.BufferLocation = dInfo->vertexBuffer->GetGPUVirtualAddress();
	dInfo->vertexBufferView.StrideInBytes = sizeof(Vertex);
	dInfo->vertexBufferView.SizeInBytes = vBufferSize;

	// Fill out the Viewport
	dInfo->viewport.TopLeftX = 0;
	dInfo->viewport.TopLeftY = 0;
	dInfo->viewport.Width = Width;
	dInfo->viewport.Height = Height;
	dInfo->viewport.MinDepth = 0.0f;
	dInfo->viewport.MaxDepth = 1.0f;

	// Fill out a scissor rect
	dInfo->scissorRect.left = 0;
	dInfo->scissorRect.top = 0;
	dInfo->scissorRect.right = Width;
	dInfo->scissorRect.bottom = Height;

	return true;
}

void UpdatePipeline(directxInfo* dInfo) {
	HRESULT hr;
	WaitForPreviousFrame(dInfo);

	//D3D12_CHECK(dInfo->commandAllocator[dInfo->frameIndex]->Reset());
	hr = dInfo->commandAllocator[dInfo->frameIndex]->Reset();
	if (FAILED(hr)) {

		Running = false;
	}

	//D3D12_CHECK(dInfo->commandList->Reset(dInfo->commandAllocator[dInfo->frameIndex], nullptr));
	hr = dInfo->commandList->Reset(dInfo->commandAllocator[dInfo->frameIndex], dInfo->pipelineStateObject);
	if (FAILED(hr)) {

		Running = false;
	}

	dInfo->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dInfo->renderTargets[dInfo->frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(dInfo->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), dInfo->frameIndex, dInfo->rtvDescriptorSize);
	
	dInfo->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	dInfo->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// draw triangle
	dInfo->commandList->SetGraphicsRootSignature(dInfo->rootSignature); // set the root signature
	dInfo->commandList->RSSetViewports(1, &dInfo->viewport); // set the viewports
	dInfo->commandList->RSSetScissorRects(1, &dInfo->scissorRect); // set the scissor rects
	dInfo->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // set the primitive topology	 ist->
	dInfo->commandList->IASetVertexBuffers(0, 1, &dInfo->vertexBufferView); // set the vertex buffer (using the vertex buffer view)
	dInfo->commandList->IASetIndexBuffer(&dInfo->indexBufferView);
	dInfo->commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
	//dInfo->commandList->DrawInstanced(3, 1, 0, 0); // finally draw 3 vertices (draw the triangle)

	dInfo->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dInfo->renderTargets[dInfo->frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	hr = dInfo->commandList->Close();
	if (FAILED(hr)) {

		Running = false;
	}
}


void Render(directxInfo* dInfo) {

	UpdatePipeline(dInfo);
	ID3D12CommandList* ppCommandLists[] = { dInfo->commandList };
	dInfo->commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	dInfo->commandQueue->Signal(dInfo->fence[dInfo->frameIndex], dInfo->fenceValue[dInfo->frameIndex]);
	dInfo->swapChain->Present(0, 0);



 }

void Cleanup(directxInfo * dInfo) {
	// wait for the gpu to finish all frames
	for (int i = 0; i < frameBufferCount; ++i)
	{
		dInfo->frameIndex = i;
		WaitForPreviousFrame(dInfo);
	}

	// get swapchain out of full screen before exiting
	BOOL fs = false;
	if (dInfo->swapChain->GetFullscreenState(&fs, NULL))
		dInfo->swapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(dInfo->device);
	SAFE_RELEASE(dInfo->swapChain);
	SAFE_RELEASE(dInfo->commandQueue);
	SAFE_RELEASE(dInfo->rtvDescriptorHeap);
	SAFE_RELEASE(dInfo->commandList);

	for (int i = 0; i < frameBufferCount; ++i)
	{
		SAFE_RELEASE(dInfo->renderTargets[i]);
		SAFE_RELEASE(dInfo->commandAllocator[i]);
		SAFE_RELEASE(dInfo->fence[i]);
	};
	SAFE_RELEASE(dInfo->pipelineStateObject);
	SAFE_RELEASE(dInfo->rootSignature);
	SAFE_RELEASE(dInfo->vertexBuffer);
	SAFE_RELEASE(dInfo->indexBuffer);
}

void WaitForPreviousFrame(directxInfo * dInfo)
{
	HRESULT hr;

	// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
	// the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
	if (dInfo->fence[dInfo->frameIndex]->GetCompletedValue() < dInfo->fenceValue[dInfo->frameIndex])
	{
		// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
		hr = dInfo->fence[dInfo->frameIndex]->SetEventOnCompletion(dInfo->fenceValue[dInfo->frameIndex], dInfo->fenceEvent);
		if (FAILED(hr))
		{
			Running = false;
		}

		// We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
		// has reached "fenceValue", we know the command queue has finished executing
		WaitForSingleObject(dInfo->fenceEvent, INFINITE);
	}

	// increment fenceValue for next frame
	dInfo->fenceValue[dInfo->frameIndex]++;

	// swap the current rtv buffer index so we draw on the correct buffer
	dInfo->frameIndex = dInfo->swapChain->GetCurrentBackBufferIndex();
}

int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd)
{
	directxInfo dInfo = {};

	// create the window
	if (!InitializeWindow(hInstance, nShowCmd, Width, Height, FullScreen))
	{
		MessageBox(0, L"Window Initialization - Failed",
			L"Error", MB_OK);
		return 0;
	}
	
	if (!InitD3D(&dInfo))
	{
		MessageBox(0, L"Failed to initialize direct3d 12",
			L"Error", MB_OK);
		Cleanup(&dInfo);
		return 1;
	}

	// start the main loop
	mainloop(&dInfo);

	// we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame(&dInfo);

	// close the fence event
	CloseHandle(dInfo.fenceEvent);

	// clean up everything
	Cleanup(&dInfo);

	return 0;
}

bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	int width, int height,
	bool fullscreen)
{
	if (fullscreen)
	{
		HMONITOR hmon = MonitorFromWindow(hwnd,
			MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		width = mi.rcMonitor.right - mi.rcMonitor.left;
		height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	WNDCLASSEX wc;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WindowName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}
	
	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop(directxInfo * dInfo) {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (Running)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// run game code
			Update(dInfo); // update the game logic
			Render(dInfo); // execute the command queue (rendering the scene is the result of the gpu executing the command lists)
		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg)
	{

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Are you sure you want to exit?",
				L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
				DestroyWindow(hwnd);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,
		msg,
		wParam,
		lParam);
}