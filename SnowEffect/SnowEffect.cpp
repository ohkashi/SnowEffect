// SnowEffect.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "SnowEffect.h"
#include "resource.h"
#include <commdlg.h>

#pragma comment(lib, "dwmapi.lib")


// Global Variables:
app_state		AppState;
static UINT		g_ResizeWidth = 0, g_ResizeHeight = 0;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance, LPCWSTR lpszWindowClass);
HWND InitInstance(HINSTANCE hInstance, LPCWSTR lpszTitle, LPCWSTR lpszWindowClass);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInst, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInst);
    UNREFERENCED_PARAMETER(lpCmdLine);

	HRESULT hr = CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
	if (FAILED(hr))
		return 1;

    // Initialize global strings
	AppState.strTitle.LoadString(hInstance, IDS_APP_TITLE);
	CString strWindowClass = AppState.strTitle + _T(" Window");
	MyRegisterClass(hInstance, strWindowClass);

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
	static CStringA strIniFilename = (LPCSTR)CT2A(AppState.strAppName + _T(".ini"));
	io.IniFilename = (LPCSTR)strIniFilename;

	// Setup Dear ImGui style
	if (AppState.isDarkMode)
		ImGui::StyleColorsDark();
	else
		ImGui::StyleColorsClassic();

	auto _ColorFromBytes = [](BYTE r, BYTE g, BYTE b) {
		return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f); };
	ImVec4(*ColorFromBytes)(BYTE r, BYTE g, BYTE b) = _ColorFromBytes;

	auto& style = ImGui::GetStyle();
	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;
	float defaultRounding = (float)MulDiv(7, AppState.dpi, 96);
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

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (AppState.show_demo_window)
			ImGui::ShowDemoWindow(&AppState.show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		{
			auto cam_pos = AppState.camera.GetPosition();

			ImGui::Begin(strTitle);                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &AppState.show_demo_window);	// Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &AppState.show_another_window);

			ImGui::SliderFloat("Zoom", &cam_pos.z, -15.0f, 0.0f);
			AppState.camera.SetPosition(cam_pos.x, cam_pos.y, cam_pos.z);
			ImGui::ColorEdit3("clear color", (float*)&AppState.clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Image")) {		// Buttons return true when clicked (most widgets return true when edited/activated)
				TCHAR szFile[MAX_PATH] = { 0 };
				OPENFILENAME ofn;
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.lpstrFilter = _T("Image files (*.jpg;*.jpeg;*.gif;*.png)\0*.jpg;*.jpeg;*.gif;*.png\0All files(*.*)\0*.*\0\0");
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST;
				if (::GetOpenFileName(&ofn)) {
					AppState.SetBackImage(szFile);
				}
			}
			if (!AppState.strBackImgPath.IsEmpty()) {
				ImGui::SameLine();
				CStringA strImgFile;
				int nPos = AppState.strBackImgPath.ReverseFind('\\');
				if (nPos != -1)
					strImgFile = AppState.strBackImgPath.Mid(nPos + 1);
				else
					strImgFile = AppState.strBackImgPath;
				ImGui::Text("File: %s", (LPCSTR)strImgFile);
			}
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (AppState.show_another_window)
		{
			ImGui::Begin("Another Window", &AppState.show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				AppState.show_another_window = false;
			ImVec2 vMin = ImGui::GetWindowContentRegionMin();
			ImVec2 vMax = ImGui::GetWindowContentRegionMax();
			float x = (ImGui::GetWindowPos().x - vMin.x) / AppState.wndSize.cx;
			float y = (ImGui::GetWindowPos().y - vMin.y) / AppState.wndSize.cy;
			ImGui::End();
			AppState.camera.SetRotation(y * -90.0f + 45.0f, 0.0f, x * -20.0f + 10.0f);
		}

		// Rendering
		ImGui::Render();
		const auto& clr = AppState.clear_color;
		const float clear_color_with_alpha[4] = { clr.x * clr.w, clr.y * clr.w, clr.z * clr.w, clr.w };
		//const float clear_color_with_alpha[4] = { 0, 0, 0, 1.0f };
		D3D::BeginScene(clear_color_with_alpha);
		AppState.Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		D3D::EndScene();

		if (io.Framerate > 60.0f)
			Sleep(10);
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	AppState.Cleanup();
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
	AppState.hInstance = hInstance; // Store instance handle in our global variable

	DWORD dwStyle = WS_OVERLAPPEDWINDOW;
	RECT rect = { 100, 100, 100 + AppState.wndSize.cx, 100 + AppState.wndSize.cy };
	AdjustWindowRectExForDpi(&rect, dwStyle, FALSE, 0, AppState.dpi);
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

	HWND hWnd = CreateWindowW(lpszWindowClass, lpszTitle, dwStyle, x, y, cxWnd, cyWnd,
							  nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return NULL;

	// Initialize Direct3D
	if (!D3D::Init(hWnd, AppState.wndSize.cx, AppState.wndSize.cy)) {
		D3D::Shutdown();
		::UnregisterClassW(lpszWindowClass, hInstance);
		return NULL;
	}

	AppState.Init(hWnd);

	return hWnd;
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
		return 0;
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
			static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };
			HWND hwnd = AppState.hWnd;
			DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
			if (dwStyle & WS_OVERLAPPEDWINDOW) {
				MONITORINFO mi = { sizeof(mi) };
				if (GetWindowPlacement(hwnd, &g_wpPrev) &&
					GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
				{
					SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
					SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
						mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
						SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
					AppState.fullScreen = true;
				}
			} else {
				SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
				SetWindowPlacement(hwnd, &g_wpPrev);
				SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
					SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
				AppState.fullScreen = false;
			}
		} else if (wParam == VK_ESCAPE) {
			PostQuitMessage(0);
		} else if (wParam == VK_F1) {
			PostMessage(AppState.hWnd, WM_COMMAND, IDM_ABOUT, 0);
		}
		return 0;
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
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
