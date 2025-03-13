// SnowEffect.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "SnowEffect.h"
#include "resource.h"
#include <commdlg.h>
#include <windowsx.h>

#pragma comment(lib, "dwmapi.lib")


// Global Variables:
app_state		AppState;
static UINT		g_ResizeWidth = 0, g_ResizeHeight = 0;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance, LPCWSTR lpszWindowClass);
HWND InitInstance(HINSTANCE hInstance, LPCWSTR lpszTitle, LPCWSTR lpszWindowClass);
void ToggleFullScreen(HWND hWnd);
BOOL BrowseBackImage(LPTSTR lpszFilePath, DWORD dwBuffLen = MAX_PATH);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

static CStringA Utf8(LPCWSTR lpszUnicode)
{
	char szUtf8[MAX_PATH] = { 0, };
	int wstr_len = min(sizeof(szUtf8) - 1, (int)wcslen(lpszUnicode));
	int nLen = WideCharToMultiByte(CP_UTF8, 0, lpszUnicode, wstr_len, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, lpszUnicode, wstr_len, szUtf8, nLen, NULL, NULL);
	return szUtf8;
}

app_state::app_state() : hInstance(NULL), hWnd(NULL), hPreview(NULL), wndSize({1024, 1024}), backSize{0}, backAspect(0),
						 isDarkMode(false), appMode(AppMode::Normal), spriteBatch(NULL), backTexture(NULL)
{
	pSettings = new AppSettings;
	isDarkMode = !IsLightTheme();
	if (isDarkMode)
		pSettings->clear_color = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
	HDC hdc = GetDC(NULL);
	dpi = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(NULL, hdc);
}

void app_state::Init(HINSTANCE hInst)
{
	this->hInstance = hInst;
	int nRet = strTitle.LoadString(hInstance, IDS_APP_TITLE);
	GetModuleFileName(hInstance, strAppDir.GetBuffer(MAX_PATH), MAX_PATH);
	strAppDir.ReleaseBuffer();
	int nPos = strAppDir.ReverseFind('\\');
	strAppName = strAppDir.Mid(nPos + 1);
	strAppDir.Delete(nPos + 1, strAppDir.GetLength() - nPos);
	nPos = strAppName.ReverseFind('.');
	CString str = strAppName.Mid(nPos + 1);
	pSettings->isScreenSaver = (str.CompareNoCase(_T("scr")) == 0);
	strAppName.Delete(nPos, strAppName.GetLength() - nPos + 1);
	pSettings->strIniFilePath = strAppDir + strAppName + _T(".ini");
	nPos = strAppName.ReverseFind('~');
	if (nPos != -1) {
		strAppName = strTitle;
		strAppName.Remove(0x20);
	}
	pSettings->Load(strAppName, appMode);
	if (__argc > 1) {
		str = __targv[1];
		if (str.GetLength() > 2)
			str.Delete(2, str.GetLength() - 2);
		if (str.CompareNoCase(_T("/s")) == 0) {
			// Run the Screen Saver.
			appMode = AppMode::ScreenSaver;
		} else if (str.CompareNoCase(_T("/c")) == 0) {
			// Show the Settings dialog box, modal to the foreground window.
			appMode = AppMode::Settings;
		} else if (str.CompareNoCase(_T("/p")) == 0) {
			// Preview Screen Saver as child of window <HWND>.
			appMode = AppMode::Preview;
			if (__argc > 2) {
				hPreview = (HWND)_ttoi(__targv[2]);
				pSettings->iSnowMax = 500;
				pSettings->show_imgui = false;
			}
		}
	} else if (pSettings->isScreenSaver) {
		appMode = AppMode::Settings;
	}

	//str.Format(_T("appMode: %d, argc: %d, argv[1]: %s"), (int)appMode, __argc, __argc > 1 ? __targv[1] : _T(""));
	//MessageBox(NULL, str, _T("Debug"), MB_ICONINFORMATION);
}

bool app_state::InitResources(HWND hwnd)
{
	this->hWnd = hwnd;
	SetTitleBarDarkMode(this->hWnd, isDarkMode);
	/*if (isDarkMode) {
	DWM_BLURBEHIND bb = { 0 };
	bb.dwFlags = DWM_BB_ENABLE;
	bb.fEnable = TRUE;
	DwmEnableBlurBehindWindow(hWnd, &bb);
	}*/
	if (appMode == AppMode::Preview)
		particle.SetSnowSize(0.1f);
	if (!particle.Init(hWnd, XMFLOAT3(0, -5, 0), pSettings->iSnowMax, 5, 5, 5)) {
		ATLTRACE("SnowParticle::Init() failed!\n");
		return false;
	}

	camera.SetPosition(0.0f, 0.0f, pSettings->zoom);
	//camera.SetRotation(0.0f, 1.0f, 1.0f);

	this->spriteBatch = new SpriteBatch(D3D::DeviceContext);
	if (!pSettings->strBackImgPath.IsEmpty())
		SetBackImage(pSettings->strBackImgPath);
	return true;
}

bool app_state::SetBackImage(LPCTSTR lpszImgPath)
{
	SafeRelease(&backTexture);
	HRESULT hr = CreateWICTextureFromFileEx(D3D::Device, lpszImgPath, 0, D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE, 0, 0, WIC_LOADER_IGNORE_SRGB, nullptr, &backTexture);
	if (SUCCEEDED(hr)) {
		ID3D11Resource* pResource = nullptr;
		backTexture->GetResource(&pResource);
		D3D11_TEXTURE2D_DESC desc;
		((ID3D11Texture2D*)pResource)->GetDesc(&desc);
		pResource->Release();
		backSize.cx = desc.Width;
		backSize.cy = desc.Height;
		backAspect = (float)backSize.cx / backSize.cy;
		pSettings->strBackImgPath = lpszImgPath;
		return true;
	}
	pSettings->strBackImgPath.Empty();
	return false;
}

void app_state::Cleanup(bool saveSettings)
{
	if (saveSettings) {
		if (appMode != AppMode::Preview)
			pSettings->Save();
	}
	SafeDelete(pSettings);
	SafeDelete(spriteBatch);
	SafeRelease(&backTexture);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInst, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInst);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr = CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
	if (FAILED(hr))
		return 1;

	AppState.Init(hInstance);
	CString strWindowClass = AppState.strTitle + _T(" Window");
	MyRegisterClass(hInstance, strWindowClass);

	if (AppState.appMode == AppMode::Settings) {
		int nResult = DialogBox(hInstance, MAKEINTRESOURCE(IDD_SETTINGS), NULL, SettingsProc);
		AppState.Cleanup(nResult == IDOK);
		return 0;
	}

    // Perform application initialization:
	HWND hWnd = InitInstance(hInstance, AppState.strTitle, strWindowClass);
	if (!hWnd)
        return 1;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls
	int nPos = AppState.settings.strIniFilePath.ReverseFind('\\');
	static CStringA strIniFilename = (LPCSTR)Utf8(AppState.settings.strIniFilePath.Mid(nPos + 1));
	if (!AppState.settings.isScreenSaver)
		io.IniFilename = (LPCSTR)strIniFilename;
	else
		io.IniFilename = NULL;
	SetCurrentDirectory(AppState.strAppDir);

	// Setup Dear ImGui style
	if (AppState.isDarkMode)
		ImGui::StyleColorsDark();
	else
		ImGui::StyleColorsClassic();

	AppState.SetBackImage(AppState.settings.strBackImgPath);

	auto _ColorFromBytes = [](BYTE r, BYTE g, BYTE b) {
		return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f); };
	ImVec4(*ColorFromBytes)(BYTE r, BYTE g, BYTE b) = _ColorFromBytes;

	auto& style = ImGui::GetStyle();
	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;
	float defaultRounding = (float)MulDiv(6, AppState.dpi, 96);
	style.WindowRounding = defaultRounding;
	style.ChildRounding = defaultRounding;
	style.FrameRounding = defaultRounding;
	style.GrabRounding = defaultRounding;
	style.PopupRounding = defaultRounding;
	style.ScrollbarRounding = defaultRounding;
	style.TabRounding = defaultRounding;
	style.FrameBorderSize = 1.0f;

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(D3D::Device, D3D::DeviceContext);
	
	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);
	LANGID lid = GetUserDefaultUILanguage();
	if (lid == 0x412) {
		io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/Malgun.ttf", (float)MulDiv(15, AppState.dpi, 96), NULL, io.Fonts->GetGlyphRangesKorean());
	} else {
		ImFontConfig fontConf;
		fontConf.SizePixels = (float)MulDiv(13, AppState.dpi, 96);
		io.Fonts->AddFontDefault(&fontConf);
	}

	CStringA strTitle = (LPCSTR)CT2A(AppState.strTitle);
	strTitle += " Options";
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SNOWEFFECT));
	if (!AppState.settings.isScreenSaver)
		SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);

	// Main loop
	bool done = false;
	while (!done) {
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
			D3D::ResizeRenderTarget(g_ResizeWidth, g_ResizeHeight);
			g_ResizeWidth = g_ResizeHeight = 0;
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (AppState.settings.show_imgui) {
			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			if (AppState.settings.show_demo_window)
				ImGui::ShowDemoWindow(&AppState.settings.show_demo_window);

			bool restore_cam_rot = AppState.settings.show_another_window;
			// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
			{
				auto cam_pos = AppState.camera.GetPosition();

				ImGui::Begin(strTitle);                          // Create a window called "Hello, world!" and append into it.

				ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
				ImGui::Checkbox("Demo Window", &AppState.settings.show_demo_window);	// Edit bools storing our window open/close state
				ImGui::Checkbox("Another Window", &AppState.settings.show_another_window);

				ImGui::SliderFloat("Zoom", &cam_pos.z, -15.0f, 0.0f);
				AppState.camera.SetPosition(cam_pos.x, cam_pos.y, cam_pos.z);
				AppState.settings.zoom = cam_pos.z;
				ImGui::ColorEdit3("clear color", (float*)&AppState.settings.clear_color); // Edit 3 floats representing a color

				if (ImGui::Button("Image")) {		// Buttons return true when clicked (most widgets return true when edited/activated)
					TCHAR szFile[MAX_PATH] = { 0 };
					if (BrowseBackImage(szFile)) {
						AppState.SetBackImage(szFile);
					}
				}
				if (!AppState.settings.strBackImgPath.IsEmpty()) {
					ImGui::SameLine();
					CStringW strImgFile;
					int nPos = AppState.settings.strBackImgPath.ReverseFind('\\');
					if (nPos != -1)
						strImgFile = AppState.settings.strBackImgPath.Mid(nPos + 1);
					else
						strImgFile = AppState.settings.strBackImgPath;
					ImGui::Text((LPCSTR)Utf8(strImgFile));
				}
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
				ImGui::End();
			}

			// 3. Show another simple window.
			if (AppState.settings.show_another_window)
			{
				ImGui::Begin("Another Window", &AppState.settings.show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
				ImGui::Text("Hello from another window!");
				if (ImGui::Button("Close Me")) {
					AppState.settings.show_another_window = false;
				} else {
					ImVec2 vMin = ImGui::GetWindowContentRegionMin();
					ImVec2 vMax = ImGui::GetWindowContentRegionMax();
					float x = (ImGui::GetWindowPos().x - vMin.x) / AppState.wndSize.cx;
					float y = (ImGui::GetWindowPos().y - vMin.y) / AppState.wndSize.cy;
					AppState.camera.SetRotation(y * 90.0f - 45.0f, 0.0f, x * -20.0f + 10.0f);
				}
				ImGui::End();
			}
			if (restore_cam_rot && !AppState.settings.show_another_window) {
				auto rot = AppState.camera.GetRotation();
				AppState.camera.SetRotation(-rot.x, 0.0f, -rot.z);
			}
		}

		// Rendering
		ImGui::Render();
		const auto& clr = AppState.settings.clear_color;
		const float clear_color_with_alpha[4] = { clr.x * clr.w, clr.y * clr.w, clr.z * clr.w, clr.w };
		//const float clear_color_with_alpha[4] = { 0, 0, 0, 1.0f };
		D3D::BeginScene(clear_color_with_alpha);
		AppState.Render(io.DeltaTime * 7.0f);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		D3D::EndScene();

		const int limitFrameRate = 1000 / 60;
		int deltaTime = (int)(io.DeltaTime * 1000.0f);
		Sleep(max(1, limitFrameRate - deltaTime));
	}

	if (!AppState.settings.isScreenSaver)
		SetThreadExecutionState(ES_CONTINUOUS);

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	AppState.Cleanup(true);
	D3D::Shutdown();
	::DestroyWindow(hWnd);

    return 0;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPCWSTR lpszWindowClass)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNOWEFFECT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)((AppState.isDarkMode ? COLOR_BACKGROUND : COLOR_WINDOW)+1);
	wcex.lpszMenuName	= NULL; //MAKEINTRESOURCEW(IDC_SNOWEFFECT);
    wcex.lpszClassName  = lpszWindowClass;
	wcex.hIconSm		= NULL; //LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

using AdjustWindowRectExForDpi_fn = BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT);

BOOL CalcWindowRectForDpi(LPRECT lpRect, DWORD  dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi, UINT nAdjustType = 0)
{
	if (nAdjustType == 0)
		dwExStyle &= ~WS_EX_CLIENTEDGE;

	HMODULE hModule = ::LoadLibrary(_T("User32.dll")); // don't call FreeLibrary() with this handle; the module was already loaded up, it would break the app
	if (hModule) {
		AdjustWindowRectExForDpi_fn addr = (AdjustWindowRectExForDpi_fn)::GetProcAddress(hModule, "AdjustWindowRectExForDpi");
		if (addr)
			return addr(lpRect, dwStyle, bMenu, dwExStyle, dpi);
	}
	return ::AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, LPCWSTR lpszTitle, LPCWSTR lpszWindowClass)
{
	DWORD dwStyle = WS_OVERLAPPEDWINDOW;
	RECT rect = { 100, 100, 100 + AppState.wndSize.cx, 100 + AppState.wndSize.cy };
	//AdjustWindowRectExForDpi(&rect, dwStyle, FALSE, 0, AppState.dpi);
	CalcWindowRectForDpi(&rect, dwStyle, FALSE, 0, AppState.dpi);
	int cxWnd, cyWnd = rect.bottom - rect.top;
	int cyFullScrn = GetSystemMetrics(SM_CYFULLSCREEN);
	if (cyWnd > cyFullScrn) {
		int diff = cyWnd - cyFullScrn;
		AppState.wndSize.cx = AppState.wndSize.cy = cyFullScrn - diff;
		rect.bottom -= diff;
		rect.right -= diff;
	}
	cxWnd = rect.right - rect.left;
	cyWnd = rect.bottom - rect.top;
	int x = (GetSystemMetrics(SM_CXFULLSCREEN) - cxWnd) / 2;
	int y = (cyFullScrn - cyWnd) / 2;
	HWND hWndParent = NULL;
	if (AppState.appMode == AppMode::Preview && AppState.hPreview) {
		hWndParent = AppState.hPreview;
		if (GetWindowRect(hWndParent, &rect)) {
			AppState.wndSize.cx = rect.right - rect.left;
			AppState.wndSize.cy = rect.bottom - rect.top;
			cxWnd = AppState.wndSize.cx;
			cyWnd = AppState.wndSize.cy;
			x = y = 0;
			dwStyle = WS_CHILD | WS_VISIBLE;
		}
	}

	HWND hWnd = CreateWindowW(lpszWindowClass, lpszTitle, dwStyle, x, y, cxWnd, cyWnd,
							  hWndParent, nullptr, hInstance, nullptr);
	if (!hWnd)
		return NULL;

	// Initialize Direct3D
	if (!D3D::Init(hWnd, AppState.wndSize.cx, AppState.wndSize.cy)) {
		D3D::Shutdown();
		::UnregisterClassW(lpszWindowClass, hInstance);
		return NULL;
	}

	if (!AppState.InitResources(hWnd))
		return NULL;
	switch (AppState.appMode) {
	case AppMode::Normal:
		if (!AppState.settings.fullScreen)
			break;
		__fallthrough;
	case AppMode::ScreenSaver:
		ToggleFullScreen(hWnd);
		break;
	case AppMode::Preview:
	case AppMode::Settings:
		break;
	}
	return hWnd;
}

void ToggleFullScreen(HWND hWnd)
{
	static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };
	DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		MONITORINFO mi = { sizeof(mi) };
		if (GetWindowPlacement(hWnd, &g_wpPrev) &&
			GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY), &mi))
		{
			SetWindowLong(hWnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			AppState.settings.fullScreen = true;
		}
	} else {
		SetWindowLong(hWnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hWnd, &g_wpPrev);
		SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		AppState.settings.fullScreen = false;
	}
}

BOOL BrowseBackImage(LPTSTR lpszFilePath, DWORD dwBuffLen)
{
	CString strCurDir;
	GetCurrentDirectory(MAX_PATH, strCurDir.GetBuffer(MAX_PATH));
	strCurDir.ReleaseBuffer();
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	CString strInitialDir;
	if (!AppState.settings.strBackImgPath.IsEmpty()) {
		strInitialDir = AppState.settings.strBackImgPath;
		int nPos = strInitialDir.ReverseFind('\\');
		if (nPos != -1)
			strInitialDir.Delete(nPos, strInitialDir.GetLength() - nPos);
		ofn.lpstrInitialDir = strInitialDir;
	}
	ofn.lpstrFilter = _T("Image files(*.jpg;*.jpeg;*.gif;*.png)\0*.jpg;*.jpeg;*.gif;*.png\0All files(*.*)\0*.*\0\0");
	ofn.lpstrFile = lpszFilePath;
	ofn.nMaxFile = dwBuffLen;
	ofn.Flags = OFN_FILEMUSTEXIST;
	BOOL bRet = ::GetOpenFileName(&ofn);
	SetCurrentDirectory(strCurDir);
	return bRet;
}


// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

    switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) {
			AppState.wndSize.cx = LOWORD(lParam);
			AppState.wndSize.cy = HIWORD(lParam);
			g_ResizeWidth = (UINT)AppState.wndSize.cx; // Queue resize
			g_ResizeHeight = (UINT)AppState.wndSize.cy;
		}
		break;
	/*case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;*/
	case WM_ERASEBKGND:
		return FALSE;
	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			if (AppState.appMode != AppMode::ScreenSaver)
				ToggleFullScreen(hWnd);
			else
				PostQuitMessage(0);
		} else if (wParam == VK_ESCAPE) {
			PostQuitMessage(0);
		} else if (wParam == VK_F1) {
			PostMessage(AppState.hWnd, WM_COMMAND, IDM_ABOUT, 0);
		} else if (wParam == VK_F2) {
			AppState.settings.show_imgui = !AppState.settings.show_imgui;
		}
		break;
	case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(AppState.hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	/*case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;*/
	case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			if (AppState.appMode == AppMode::ScreenSaver && !AppState.settings.show_imgui) {
				static POINT ptPrePos = { -999, -999 };
				if (ptPrePos.x > -999) {
					int xDelta = xPos - ptPrePos.x;
					int yDelta = yPos - ptPrePos.y;
					if (abs(xDelta) > 20 || abs(yDelta) > 20)
						PostQuitMessage(0);
				}
				ptPrePos = { xPos, yPos };
			}
		}
		break;
	default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CenterDialog(HWND hDlg)
{
	HWND hwndOwner;
	RECT rc, rcDlg, rcOwner;

	// Get the owner window and dialog box rectangles. 
	if ((hwndOwner = GetParent(hDlg)) == NULL) 
		hwndOwner = GetDesktopWindow(); 

	GetWindowRect(hwndOwner, &rcOwner); 
	GetWindowRect(hDlg, &rcDlg); 
	CopyRect(&rc, &rcOwner); 

	// Offset the owner and dialog box rectangles so that right and bottom 
	// values represent the width and height, and then offset the owner again 
	// to discard space taken up by the dialog box. 
	OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
	OffsetRect(&rc, -rc.left, -rc.top); 
	OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

	// The new position is the sum of half the remaining space and the owner's 
	// original position.
	SetWindowPos(hDlg, HWND_TOP, rcOwner.left + (rc.right / 2), rcOwner.top + (rc.bottom / 2), 0, 0, SWP_NOSIZE);
}

INT_PTR CALLBACK SettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	DWORD dwPos;
	HWND hwndTrack;
	CString str;

	switch (message) {
	case WM_INITDIALOG:
		{
			//AppState.SetTitleBarDarkMode(hDlg, AppState.isDarkMode);
			HICON hIcon = LoadIcon(AppState.hInstance, MAKEINTRESOURCE(IDI_SNOWEFFECT));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
			CenterDialog(hDlg);
			SetDlgItemInt(hDlg, IDC_EDIT_PARTICLE_MAX, AppState.settings.iSnowMax, FALSE);
			SendMessage(GetDlgItem(hDlg, IDC_SPIN1), UDM_SETRANGE, 0, MAKELPARAM(5000, 100));
			str.Format(_T("%.2f"), AppState.settings.zoom);
			SetDlgItemText(hDlg, IDC_EDIT_ZOOM, str);
			HWND hSlider = GetDlgItem(hDlg, IDC_SLIDER1);
			SendMessage(hSlider, TBM_SETRANGE, FALSE, MAKELPARAM(0, 15));
			SendMessage(hSlider, TBM_SETPOS, TRUE, (LPARAM)(AppState.settings.zoom + 15.0f));
			Button_SetCheck(GetDlgItem(hDlg, IDC_CHECK_SHOW_IMGUI), AppState.settings.show_imgui ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemText(hDlg, IDC_EDIT_IMAGE_PATH, AppState.settings.strBackImgPath);
		}
		return (INT_PTR)TRUE;
	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			switch (GetDlgCtrlID((HWND)lParam)) {
			case IDC_EDIT_ZOOM:
			case IDC_EDIT_IMAGE_PATH:
				SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
				SetBkColor(hdcStatic, GetSysColor(COLOR_WINDOW));
				return (INT_PTR)GetStockObject(WHITE_BRUSH);
			}
			return DefWindowProc(hDlg, message, wParam, lParam);
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			AppState.settings.iSnowMax = GetDlgItemInt(hDlg, IDC_EDIT_PARTICLE_MAX, NULL, FALSE);
			AppState.settings.show_imgui = Button_GetCheck(GetDlgItem(hDlg, IDC_CHECK_SHOW_IMGUI));
			GetDlgItemText(hDlg, IDC_EDIT_IMAGE_PATH, AppState.settings.strBackImgPath.GetBuffer(MAX_PATH), MAX_PATH);
			AppState.settings.strBackImgPath.ReleaseBuffer();
			__fallthrough;
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		case IDC_BTN_BROWSE:
			{
				TCHAR szFile[MAX_PATH] = { 0 };
				if (BrowseBackImage(szFile)) {
					SetDlgItemText(hDlg, IDC_EDIT_IMAGE_PATH, szFile);
				}
			}
			return (INT_PTR)TRUE;
		}
		break;
	case WM_HSCROLL:
		hwndTrack = (HWND)lParam;
		switch (LOWORD(wParam)) { 
		case TB_THUMBTRACK:
		case TB_ENDTRACK:
			dwPos = SendMessage(hwndTrack, TBM_GETPOS, 0, 0);
			AppState.settings.zoom = dwPos - 15.0f;
			str.Format(_T("%.2f"), AppState.settings.zoom);
			SetDlgItemText(hDlg, IDC_EDIT_ZOOM, str);
			break;
		default:
			break;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
    case WM_INITDIALOG:
		//AppState.SetTitleBarDarkMode(hDlg, AppState.isDarkMode);
		CenterDialog(hDlg);
		return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
