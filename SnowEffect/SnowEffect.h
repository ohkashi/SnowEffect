#pragma once

#include "SnowParticle.h"
#include "D3DCamera.h"
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <WinReg.hpp>
#include <dwmapi.h>


enum class AppMode { Normal, ScreenSaver, Preview, Settings };

class AppSettings;

struct app_state {
	app_state();

	void Init(HINSTANCE hInst);
	bool InitResources(HWND hwnd);
	bool SetBackImage(LPCTSTR lpszImgPath);
	void Cleanup(bool saveSettings);

	bool IsLightTheme() {
		DWORD dwData = 0;
		DWORD cbData = sizeof(dwData);
		auto res = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &dwData, &cbData);
		if (res != ERROR_SUCCESS)
			return true;
		return (dwData != 0);
	}

	bool SetTitleBarDarkMode(HWND hWnd, bool bDarkMode) {
		BOOL value = bDarkMode;
		HRESULT hr = ::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
		if (SUCCEEDED(hr)) {
			LONG lStyle = GetWindowLong(hWnd, GWL_STYLE);
			if (lStyle & WS_CAPTION) {
				int value = (int)DWM_SYSTEMBACKDROP_TYPE::DWMSBT_TRANSIENTWINDOW;
				DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
			}
			return true;
		}
		return false;
	}

	void Render(float scalar = 0.1f) {
		camera.Render();
		if (backTexture) {
			int back_width = (int)(wndSize.cy * backAspect);
			RECT destRect = { (wndSize.cx - back_width) / 2, 0, 0, wndSize.cy };
			destRect.right = destRect.left + back_width;
			spriteBatch->Begin();
			spriteBatch->Draw(backTexture, destRect);
			spriteBatch->End();
		}
		particle.Update(scalar);

		// Turn on the alpha blending.
		D3D::EnableBlending();
		particle.Render(wndSize.cx, wndSize.cy);
		XMMATRIX projectionMatrix = D3D::ProjectionMatrix;
		particle.RenderShader(D3D::WorldMatrix, camera.GetViewMatrix(), projectionMatrix);
		D3D::EnableBlending(false);
	}

	inline AppSettings& GetSettings() const { return *pSettings; }
	__declspec(property(get = GetSettings)) AppSettings& settings;

	HINSTANCE	hInstance;
	UINT		dpi;
	HWND		hWnd, hPreview;
	SIZE		wndSize;
	AppMode		appMode;
	bool		isDarkMode;
	CString		strTitle;
	CString		strAppDir;
	CString		strAppName;
	SIZE		backSize;
	float		backAspect;

	AppSettings*	pSettings;
	SnowParticle	particle;
	D3DCamera		camera;
	SpriteBatch*	spriteBatch;
	ID3D11ShaderResourceView*	backTexture;
};

extern app_state	AppState;

class AppSettings {
public:
	AppSettings() : isScreenSaver(false), fullScreen(false), iSnowMax(3000), zoom(-7.0f),
		show_imgui(true), show_demo_window(false), show_another_window(false)
	{
		clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.0f);
	}

	void Load(LPCTSTR lpszSubKey, AppMode mode) {
		if (isScreenSaver) {
			winreg::RegKey key;
			subKey = L"SOFTWARE\\Makjangsoft\\";
			subKey += (LPCWSTR)CT2W(lpszSubKey);
			subKey += L".scr";
			auto res = key.TryOpen(HKEY_CURRENT_USER, subKey, KEY_READ | KEY_WOW64_64KEY);
			if (res.IsOk()) {
				fullScreen = key.GetDwordValue(L"FullScreen");
				auto vtColor = key.GetBinaryValue(L"ClearColor");
				CopyMemory(&clear_color, &vtColor[0], sizeof(clear_color));
				auto str = key.GetStringValue(L"Zoom");
				zoom = (float)_wtof(str.c_str());
				iSnowMax = (int)key.GetDwordValue(L"ParticleMax");
				show_imgui = key.GetDwordValue(L"ShowImGui");
				show_demo_window = key.GetDwordValue(L"ShowDemoWindow");
				show_another_window = key.GetDwordValue(L"ShowAnotherWindow");
				strBackImgPath = key.GetStringValue(L"ImagePath").c_str();
				key.Close();
			} else {
				key.Open(HKEY_CURRENT_USER, L"Control Panel\\Desktop", KEY_READ | KEY_WOW64_64KEY);
				strBackImgPath = key.GetStringValue(L"WallPaper").c_str();
				key.Close();
				Save();
			}
		} else {
			fullScreen = ReadIniInt(lpszIniSection, _T("FullScreen"), mode == AppMode::ScreenSaver);
			CString str, strColor = ReadIniString(lpszIniSection, _T("ClearColor"), 20);
			strColor.Trim();
			if (!strColor.IsEmpty()) {
				int i, iPos = 0;
				float color[4] = { 0 };
				for (i = 0; i < 3; i++) {
					str = strColor.Tokenize(_T(","), iPos);
					color[i] = (float)_ttof(str);
					if (iPos == -1)
						break;
				}
				clear_color.x = color[0];
				clear_color.y = color[1];
				clear_color.z = color[2];
			}
			str = ReadIniString(lpszIniSection, _T("Zoom"), 10);
			if (!str.IsEmpty())
				zoom = (float)_ttof(str);
			iSnowMax = ReadIniInt(lpszIniSection, _T("ParticleMax"), iSnowMax);
			show_imgui = ReadIniInt(lpszIniSection, _T("ShowImGui"), mode == AppMode::Normal);
			strBackImgPath = ReadIniString(lpszIniSection, _T("ImagePath"));
			strBackImgPath.Trim();
			if (strBackImgPath.IsEmpty())
				strBackImgPath = AppState.strAppDir + AppState.strAppName + _T(".jpg");
		}
	}

	void Save() {
		if (isScreenSaver) {
			winreg::RegKey key;
			auto res = key.TryCreate(HKEY_CURRENT_USER, subKey);
			if (res.IsOk()) {
				key.SetDwordValue(L"FullScreen", fullScreen);
				key.SetBinaryValue(L"ClearColor", &clear_color, sizeof(clear_color));
				CStringW str;
				str.Format(L"%.2f", zoom);
				key.SetStringValue(L"Zoom", (LPCWSTR)str);
				key.SetDwordValue(L"ParticleMax", iSnowMax);
				key.SetDwordValue(L"ShowImGui", show_imgui);
				key.SetDwordValue(L"ShowDemoWindow", show_demo_window);
				key.SetDwordValue(L"ShowAnotherWindow", show_another_window);
				key.SetStringValue(L"ImagePath", (LPCWSTR)strBackImgPath);
				key.Close();
			}
		} else {
			WriteIniInt(lpszIniSection, _T("FullScreen"), fullScreen);
			CString str;
			str.Format(_T("%.2f,%.2f,%.2f"), clear_color.x, clear_color.y, clear_color.z);
			WriteIniString(lpszIniSection, _T("ClearColor"), str);
			str.Format(_T("%.2f"), zoom);
			WriteIniString(lpszIniSection, _T("Zoom"), str);
			WriteIniInt(lpszIniSection, _T("ParticleMax"), iSnowMax);
			WriteIniInt(lpszIniSection, _T("ShowImGui"), show_imgui);
			WriteIniInt(lpszIniSection, _T("ShowDemoWindow"), show_demo_window);
			WriteIniInt(lpszIniSection, _T("ShowAnotherWindow"), show_another_window);
			WriteIniString(lpszIniSection, _T("ImagePath"), strBackImgPath);
		}
	}

	bool	isScreenSaver;
	bool	fullScreen;
	ImVec4	clear_color;
	bool	show_imgui;
	bool	show_demo_window;
	bool	show_another_window;
	int		iSnowMax;
	float	zoom;
	CString	strBackImgPath;
	std::wstring	subKey;
	CString	strIniFilePath;
	constexpr static LPCTSTR lpszIniSection = _T("UserSettings");

protected:
	CString ReadIniString(LPCTSTR lpszSection, LPCTSTR lpszKeyName, DWORD nSize = MAX_PATH) {
		CString strValue;
		GetPrivateProfileString(lpszSection, lpszKeyName, NULL, strValue.GetBuffer(nSize), nSize, strIniFilePath);
		strValue.ReleaseBuffer();
		return strValue;
	}
	int ReadIniInt(LPCTSTR lpszSection, LPCTSTR lpszKeyName, int nDefault = 0) {
		CString strValue;
		GetPrivateProfileString(lpszSection, lpszKeyName, NULL, strValue.GetBuffer(16), 16, strIniFilePath);
		strValue.ReleaseBuffer();
		if (strValue.IsEmpty())
			return nDefault;
		return _ttoi(strValue);
	}
	bool WriteIniString(LPCTSTR lpszSection, LPCTSTR lpszKeyName, LPCTSTR lpszString) {
		return WritePrivateProfileString(lpszSection, lpszKeyName, lpszString, strIniFilePath);
	}
	bool WriteIniInt(LPCTSTR lpszSection, LPCTSTR lpszKeyName, int nValue) {
		CString strValue;
		strValue.Format(_T("%d"), nValue);
		return WritePrivateProfileString(lpszSection, lpszKeyName, strValue, strIniFilePath);
	}
};
