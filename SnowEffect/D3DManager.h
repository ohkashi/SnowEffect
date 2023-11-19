#pragma once

#include <DirectXMath.h>

using namespace DirectX;

namespace D3D {
	bool Init(HWND hwnd, int cxScreen, int cyScreen, bool vsync = true, bool fullscreen = false,
			  float screenDepth = 1000.0f, float screenNear = 0.1f);
	void Shutdown();
	void BeginScene(const float clear_color[]);
	void EndScene();
	bool ResizeRenderTarget(UINT Width, UINT Height, DXGI_FORMAT NewFormat = DXGI_FORMAT_UNKNOWN);
	void GetVideoCardInfo(char* cardName, int& memory);
	void EnableBlending(bool bEnable = true);

	ID3D11RenderTargetView* GetRenderTargetView();

	extern ID3D11Device*		Device;
	extern ID3D11DeviceContext*	DeviceContext;
	extern IDXGISwapChain*		SwapChain;
	extern XMMATRIX				ProjectionMatrix;
	extern XMMATRIX				WorldMatrix;
	extern XMMATRIX				OrthoMatrix;
}
