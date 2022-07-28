#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"


// Handle to the window
HWND hwnd = NULL;

// name of the window (not the title)
LPCTSTR WindowName = L"BzTutsApp";

// title of the window
LPCTSTR WindowTitle = L"Bz Window";

// width and height of the window
int Width = 800;
int Height = 600;
bool Running = true;
// is window full screen?
bool FullScreen = false;

// create a window
bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	int width, int height,
	bool fullscreen);

// main application loop


// callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);
const int frameBufferCount = 3;
struct directxInfo {
	
	ID3D12CommandQueue* commandQueue;
	ID3D12Device* device;
	IDXGISwapChain3* swapChain;
	ID3D12DescriptorHeap* rtvDescriptorHeap;
	ID3D12Resource* renderTargets[frameBufferCount];
	ID3D12CommandAllocator* commandAllocator[frameBufferCount];
	ID3D12GraphicsCommandList* commandList;
	ID3D12Fence* fence[frameBufferCount];
	HANDLE fenceEvent;
	UINT64 fenceValue[frameBufferCount];
	int frameIndex;
	int rtvDescriptorSize;


};

void mainloop(directxInfo* dInfo);

bool InitD3D(directxInfo* dInfo);
void Update(directxInfo* dInfo) {};
void UpdatePipeline(directxInfo * dInfo); // update the direct3d pipeline (update command lists)

void Render(directxInfo* dInfo); // execute the command list

void Cleanup(directxInfo * dInfo); // release com ojects and clean up memory

void WaitForPreviousFrame(directxInfo* dInfo); // wait until gpu is finished with command list
