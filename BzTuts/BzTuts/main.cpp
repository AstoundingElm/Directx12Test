#include "stdafx.h"
#include <stdio.h>

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

#define D3D12_CHECK(result) \
if(result == S_OK) \
{          \
printf("Directx12 Error: %s%d", __FILE__, __LINE__); \
__debugbreak();								\
return false;};									\

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
	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter)!= DXGI_ERROR_NOT_FOUND) {
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
	D3D12_CHECK(dInfo->commandList->Close());

	for (int i = 0; i < frameBufferCount; i++) {

		D3D12_CHECK(dInfo->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dInfo->fence[i])));
		dInfo->fenceValue[i] = 0;
	}

	dInfo->fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); //event name?



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
	hr = dInfo->commandList->Reset(dInfo->commandAllocator[dInfo->frameIndex], nullptr);
	if (FAILED(hr)) {

		Running = false;
	}

	dInfo->commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(dInfo->renderTargets[dInfo->frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(dInfo->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), dInfo->frameIndex, dInfo->rtvDescriptorSize);
	
	dInfo->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
	const float clearColour[] = {1.0f, 0.2f, 0.4f, 1.0f};
	dInfo->commandList->ClearRenderTargetView(rtvHandle, clearColour, 0, nullptr);
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