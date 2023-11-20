#include "pch.h"
#include "D3DManager.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

ID3D11Device*			D3D::Device = nullptr;
ID3D11DeviceContext*	D3D::DeviceContext = nullptr;
IDXGISwapChain*			D3D::SwapChain = nullptr;
XMMATRIX				D3D::ProjectionMatrix;
XMMATRIX				D3D::WorldMatrix;
XMMATRIX				D3D::OrthoMatrix;

class D3DManager
{
public:
	D3DManager() : m_vsync_enabled(true), m_videoCardMemory(0), m_videoCardDescription({0}), m_renderTargetView(NULL),
				   m_scrnNear(0), m_scrnDepth(0), m_depthStencilBuffer(NULL), m_depthStencilState(NULL), m_depthStencilView(NULL),
				   m_rasterState(NULL), m_alphaEnableBlendingState(NULL), m_alphaDisableBlendingState(NULL)	{}
	~D3DManager() {}

	bool Initialize(HWND hwnd, int cxScreen, int cyScreen, bool vsync, bool fullscreen, float screenDepth, float screenNear) {
		IDXGIFactory* factory;
		IDXGIAdapter* adapter;
		IDXGIOutput* adapterOutput;
		unsigned int numModes = 0, numerator = 0, denominator = 1, stringLength = 0;
		DXGI_MODE_DESC* displayModeList;
		DXGI_ADAPTER_DESC adapterDesc;
		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		D3D_FEATURE_LEVEL featureLevel;
		ID3D11Texture2D* backBufferPtr;
		D3D11_RASTERIZER_DESC rasterDesc;
		D3D11_VIEWPORT viewport;
		float fieldOfView, screenAspect;
		D3D11_BLEND_DESC blendStateDescription;

		// Store the vsync setting.
		m_vsync_enabled = vsync;
		m_scrnNear = screenNear;
		m_scrnDepth = screenDepth;

		// Create a DirectX graphics interface factory.
		HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
		if (FAILED(hr))
			return false;

		// Use the factory to create an adapter for the primary graphics interface (video card).
		hr = factory->EnumAdapters(0, &adapter);
		if (FAILED(hr))
			return false;

		// Enumerate the primary adapter output (monitor).
		hr = adapter->EnumOutputs(0, &adapterOutput);
		if (FAILED(hr))
			return false;

		// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
		hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
		if (FAILED(hr))
			return false;

		// Create a list to hold all the possible display modes for this monitor/video card combination.
		displayModeList = new DXGI_MODE_DESC[numModes];
		if (!displayModeList)
			return false;

		// Now fill the display mode list structures.
		hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
		if (FAILED(hr))
			return false;

		// Now go through all the display modes and find the one that matches the screen width and height.
		// When a match is found store the numerator and denominator of the refresh rate for that monitor.
		int i;
		for (i = 0; i < (int)numModes; i++) {
			if (displayModeList[i].Width == (UINT)cxScreen) {
				if (displayModeList[i].Height == (UINT)cyScreen) {
					numerator = displayModeList[i].RefreshRate.Numerator;
					denominator = displayModeList[i].RefreshRate.Denominator;
				}
			}
		}

		// Get the adapter (video card) description.
		hr = adapter->GetDesc(&adapterDesc);
		if (FAILED(hr))
			return false;

		// Store the dedicated video card memory in megabytes.
		m_videoCardMemory = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

		// Convert the name of the video card to a character array and store it.
		errno_t error = wcstombs_s(&stringLength, m_videoCardDescription, 128, adapterDesc.Description, 128);
		if (error != 0)
			return false;

		// Release the display mode list.
		delete[] displayModeList;
		displayModeList = nullptr;

		// Release the adapter output.
		adapterOutput->Release();
		adapterOutput = nullptr;

		// Release the adapter.
		adapter->Release();
		adapter = nullptr;

		// Release the factory.
		factory->Release();
		factory = nullptr;

		// Initialize the swap chain description.
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

		// Set to a single back buffer.
		swapChainDesc.BufferCount = 1;

		// Set the width and height of the back buffer.
		swapChainDesc.BufferDesc.Width = cxScreen;
		swapChainDesc.BufferDesc.Height = cyScreen;

		// Set regular 32-bit surface for the back buffer.
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		// Set the refresh rate of the back buffer.
		if (m_vsync_enabled) {
			swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
		} else {
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		}

		// Set the usage of the back buffer.
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

		// Set the handle for the window to render to.
		swapChainDesc.OutputWindow = hwnd;

		// Turn multisampling off.
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;

		// Set to full screen or windowed mode.
		swapChainDesc.Windowed = !fullscreen;

		// Set the scan line ordering and scaling to unspecified.
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		// Discard the back buffer contents after presenting.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		// Don't set the advanced flags.
		swapChainDesc.Flags = 0;

		// Set the feature level to DirectX 11.
		featureLevel = D3D_FEATURE_LEVEL_11_0;

		// Create the swap chain, Direct3D device, and Direct3D device context.
		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
			D3D11_SDK_VERSION, &swapChainDesc, &D3D::SwapChain, &D3D::Device, NULL, &D3D::DeviceContext);
		if (FAILED(hr))
			return false;

		// Get the pointer to the back buffer.
		hr = D3D::SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferPtr);
		if (FAILED(hr))
			return false;

		// Create the render target view with the back buffer pointer.
		hr = D3D::Device->CreateRenderTargetView(backBufferPtr, NULL, &m_renderTargetView);
		if (FAILED(hr))
			return false;

		// Release pointer to the back buffer as we no longer need it.
		backBufferPtr->Release();
		backBufferPtr = nullptr;

		hr = CreateDepthStencilView(cxScreen, cyScreen);
		if (FAILED(hr))
			return false;

		// Setup the raster description which will determine how and what polygons will be drawn.
		rasterDesc.AntialiasedLineEnable = false;
		rasterDesc.CullMode = D3D11_CULL_BACK;
		rasterDesc.DepthBias = 0;
		rasterDesc.DepthBiasClamp = 0.0f;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.ScissorEnable = false;
		rasterDesc.SlopeScaledDepthBias = 0.0f;

		// Create the rasterizer state from the description we just filled out.
		hr = D3D::Device->CreateRasterizerState(&rasterDesc, &m_rasterState);
		if (FAILED(hr))
			return false;

		// Now set the rasterizer state.
		D3D::DeviceContext->RSSetState(m_rasterState);

		// Setup the viewport for rendering.
		viewport.Width = (float)cxScreen;
		viewport.Height = (float)cxScreen;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;

		// Create the viewport.
		D3D::DeviceContext->RSSetViewports(1, &viewport);

		// Setup the projection matrix.
		fieldOfView = XM_PI / 4.0f;
		screenAspect = viewport.Width / viewport.Height;

		// Create the projection matrix for 3D rendering.
		D3D::ProjectionMatrix = XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, screenNear, screenDepth);

		// Initialize the world matrix to the identity matrix.
		D3D::WorldMatrix = XMMatrixIdentity();

		// Create an orthographic projection matrix for 2D rendering.
		D3D::OrthoMatrix = XMMatrixOrthographicLH(viewport.Width, viewport.Height, screenNear, screenDepth);

		// Clear the blend state description.
		ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));

		// Create an alpha enabled blend state description.
		for (i = 0; i < 8; i++) {
			blendStateDescription.RenderTarget[i].BlendEnable = TRUE;
			blendStateDescription.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
			blendStateDescription.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
			blendStateDescription.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			blendStateDescription.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
			blendStateDescription.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
			blendStateDescription.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blendStateDescription.RenderTarget[i].RenderTargetWriteMask = 0x0f;
		}
		// Create the blend state using the description.
		hr = D3D::Device->CreateBlendState(&blendStateDescription, &m_alphaEnableBlendingState);
		if (FAILED(hr))
			return false;

		// Modify the description to create an alpha disabled blend state description.
		for (i = 0; i < 8; i++)
			blendStateDescription.RenderTarget[i].BlendEnable = FALSE;

		// Create the blend state using the description.
		hr = D3D::Device->CreateBlendState(&blendStateDescription, &m_alphaDisableBlendingState);
		return SUCCEEDED(hr);
	}

	void Shutdown() {
		// Before shutting down set to windowed mode or when you release the swap chain it will throw an exception.
		if (D3D::SwapChain)
			D3D::SwapChain->SetFullscreenState(false, NULL);
		SafeRelease(&m_alphaEnableBlendingState);
		SafeRelease(&m_alphaDisableBlendingState);
		SafeRelease(&m_rasterState);
		SafeRelease(&m_depthStencilView);
		SafeRelease(&m_depthStencilState);
		SafeRelease(&m_depthStencilBuffer);
		SafeRelease(&m_renderTargetView);
		SafeRelease(&D3D::DeviceContext);
		SafeRelease(&D3D::Device);
		SafeRelease(&D3D::SwapChain);
	}

	void BeginScene(const float clear_clr[]) {
		// Clear the back buffer.
		D3D::DeviceContext->ClearRenderTargetView(m_renderTargetView, clear_clr);

		// Clear the depth buffer.
		D3D::DeviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		D3D::DeviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
	}

	void EndScene() {
		// Present the back buffer to the screen since rendering is complete.
		HRESULT hr = D3D::SwapChain->Present((UINT)m_vsync_enabled, 0);
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
			HandleDeviceLost();
		}
	}

	bool ResizeRenderTarget(UINT width, UINT height) {
		D3D::DeviceContext->OMSetRenderTargets(0, 0, 0);
		SafeRelease(&m_renderTargetView);
		SafeRelease(&m_depthStencilView);
		HRESULT hr = D3D::SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
			HandleDeviceLost();
			return false;
		}

		ID3D11Texture2D* pBackBuffer;
		hr = D3D::SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		if (SUCCEEDED(hr)) {
			hr = D3D::Device->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
			pBackBuffer->Release();
			if (SUCCEEDED(hr)) {
				hr = CreateDepthStencilView(width, height);
				if (SUCCEEDED(hr)) {
					// Setup the viewport for rendering.
					D3D11_VIEWPORT viewport;
					viewport.Width = (float)width;
					viewport.Height = (float)height;
					viewport.MinDepth = 0.0f;
					viewport.MaxDepth = 1.0f;
					viewport.TopLeftX = 0.0f;
					viewport.TopLeftY = 0.0f;

					// Create the viewport.
					D3D::DeviceContext->RSSetViewports(1, &viewport);
					float fieldOfView = XM_PI / 4.0f;
					float screenAspect = viewport.Width / viewport.Height;
					D3D::ProjectionMatrix = XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, m_scrnNear, m_scrnDepth);
					D3D::WorldMatrix = XMMatrixIdentity();
					D3D::OrthoMatrix = XMMatrixOrthographicLH(viewport.Width, viewport.Height, m_scrnNear, m_scrnDepth);
				}
			}
		}
		return SUCCEEDED(hr);
	}

	void GetVideoCardInfo(char* cardName, int& memory) const {
		strcpy_s(cardName, 128, m_videoCardDescription);
		memory = m_videoCardMemory;
	}

	void EnableAlphaBlending() {
		// Setup the blend factor.
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Turn on the alpha blending.
		D3D::DeviceContext->OMSetBlendState(m_alphaEnableBlendingState, blendFactor, 0xffffffff);
	}

	void DisableAlphaBlending() {
		// Setup the blend factor.
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Turn off the alpha blending.
		D3D::DeviceContext->OMSetBlendState(m_alphaDisableBlendingState, blendFactor, 0xffffffff);
	}

	inline ID3D11RenderTargetView* GetRenderTargetView() const {
		return m_renderTargetView;
	}

protected:
	HRESULT CreateDepthStencilView(UINT width, UINT height) {
		// Initialize the description of the depth buffer.
		D3D11_TEXTURE2D_DESC depthBufferDesc;
		ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

		// Set up the description of the depth buffer.
		depthBufferDesc.Width = width;
		depthBufferDesc.Height = height;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.ArraySize = 1;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags = 0;
		depthBufferDesc.MiscFlags = 0;

		// Create the texture for the depth buffer using the filled out description.
		HRESULT hr = D3D::Device->CreateTexture2D(&depthBufferDesc, NULL, &m_depthStencilBuffer);
		while (SUCCEEDED(hr)) {
			// Initialize the description of the stencil state.
			D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
			ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

			// Set up the description of the stencil state.
			depthStencilDesc.DepthEnable = true;
			depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

			depthStencilDesc.StencilEnable = true;
			depthStencilDesc.StencilReadMask = 0xFF;
			depthStencilDesc.StencilWriteMask = 0xFF;

			// Stencil operations if pixel is front-facing.
			depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
			depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Stencil operations if pixel is back-facing.
			depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
			depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

			// Create the depth stencil state.
			hr = D3D::Device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
			if (FAILED(hr))
				break;

			// Set the depth stencil state.
			D3D::DeviceContext->OMSetDepthStencilState(m_depthStencilState, 1);

			// Initialize the depth stencil view.
			D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
			ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

			// Set up the depth stencil view description.
			depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			depthStencilViewDesc.Texture2D.MipSlice = 0;

			// Create the depth stencil view.
			hr = D3D::Device->CreateDepthStencilView(m_depthStencilBuffer, &depthStencilViewDesc, &m_depthStencilView);
			if (FAILED(hr))
				break;

			// Bind the render target view and depth stencil buffer to the output render pipeline.
			D3D::DeviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);
			break;
		}
		return hr;
	}

	void HandleDeviceLost() {
		HRESULT reason = D3D::Device->GetDeviceRemovedReason();
		ATLTRACE("Device removed! DXGI_ERROR code: 0x%X\n", reason);
	}

private:
	bool	m_vsync_enabled;
	int		m_videoCardMemory;
	char	m_videoCardDescription[128];
	float	m_scrnNear, m_scrnDepth;
	ID3D11RenderTargetView*	m_renderTargetView;
	ID3D11Texture2D*		m_depthStencilBuffer;
	ID3D11DepthStencilState* m_depthStencilState;
	ID3D11DepthStencilView*	m_depthStencilView;
	ID3D11RasterizerState*	m_rasterState;
	ID3D11BlendState*		m_alphaEnableBlendingState;
	ID3D11BlendState*		m_alphaDisableBlendingState;
};

static D3DManager	g_d3dMgr;

// D3D namespace

bool D3D::Init(HWND hwnd, int cxScreen, int cyScreen, bool vsync, bool fullscreen, float screenDepth, float screenNear)
{
	return g_d3dMgr.Initialize(hwnd, cxScreen, cyScreen, vsync, fullscreen, screenDepth, screenNear);
}

void D3D::Shutdown()
{
	g_d3dMgr.Shutdown();
}

void D3D::BeginScene(const float clear_color[])
{
	g_d3dMgr.BeginScene(clear_color);
}

void D3D::EndScene()
{
	g_d3dMgr.EndScene();
}

bool D3D::ResizeRenderTarget(UINT width, UINT height)
{
	return g_d3dMgr.ResizeRenderTarget(width, height);
}

void D3D::GetVideoCardInfo(char* cardName, int& memory)
{
	g_d3dMgr.GetVideoCardInfo(cardName, memory);
}

void D3D::EnableBlending(bool bEnable)
{
	if (bEnable)
		g_d3dMgr.EnableAlphaBlending();
	else
		g_d3dMgr.DisableAlphaBlending();
}

ID3D11RenderTargetView* D3D::GetRenderTargetView()
{
	return g_d3dMgr.GetRenderTargetView();
}
