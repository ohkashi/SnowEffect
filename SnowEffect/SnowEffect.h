#pragma once

#include "SnowParticle.h"
#include "D3DCamera.h"
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <dwmapi.h>


struct app_state {
	app_state() : hInstance(NULL), hWnd(NULL), wndSize({1024, 1024}), isDarkMode(false), isScrnSaver(false),
		fullScreen(false), show_imgui(true), show_demo_window(false), show_another_window(false),
		iSnowMax(3000), spriteBatch(NULL), backSize{0}, backTexture(NULL)
	{
		isDarkMode = !IsLightTheme();
		clear_color = isDarkMode ? ImVec4(0.15f, 0.15f, 0.15f, 1.0f) : ImVec4(0.45f, 0.55f, 0.60f, 1.0f);
		HDC hdc = GetDC(NULL);
		dpi = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(NULL, hdc);

		GetModuleFileName(hInstance, strAppDir.GetBuffer(MAX_PATH), MAX_PATH);
		strAppDir.ReleaseBuffer();
		int nPos = strAppDir.ReverseFind('\\');
		strAppName = strAppDir.Mid(nPos + 1);
		strAppDir.Delete(nPos + 1, strAppDir.GetLength() - nPos);
		nPos = strAppName.ReverseFind('.');
		CString str = strAppName.Mid(nPos + 1);
		isScrnSaver = (str.CompareNoCase(_T("scr")) == 0);
		strAppName.Delete(nPos, strAppName.GetLength() - nPos + 1);
		strIniFilePath = strAppDir + strAppName + _T(".ini");
		show_imgui = ReadIniInt(_T("UserSettings"), _T("ShowImGui"), TRUE);
		fullScreen = ReadIniInt(_T("UserSettings"), _T("FullScreen"), FALSE);
		iSnowMax = ReadIniInt(_T("UserSettings"), _T("ParticleMax"), iSnowMax);
	}

	bool Init(HWND hwnd) {
		this->hWnd = hwnd;

		SetTitleBarDarkMode(this->hWnd, isDarkMode);
		/*if (isDarkMode) {
			DWM_BLURBEHIND bb = { 0 };
			bb.dwFlags = DWM_BB_ENABLE;
			bb.fEnable = TRUE;
			DwmEnableBlurBehindWindow(hWnd, &bb);
		}*/
		if (!particle.Init(hWnd, XMFLOAT3(0, -5, 0), iSnowMax, 5, 5, 5)) {
			ATLTRACE("SnowParticle::Init() failed!\n");
			return false;
		}

		float zoom = -particle.GetViewSize().z - 2.5f;
		CString str = ReadIniString(_T("UserSettings"), _T("Zoom"), 10);
		if (!str.IsEmpty())
			zoom = (float)_ttof(str);
		camera.SetPosition(0.0f, 0.0f, zoom);
		//camera.SetRotation(0.0f, 1.0f, 1.0f);

		this->spriteBatch = new SpriteBatch(D3D::DeviceContext);
		if (!strBackImgPath.IsEmpty())
			SetBackImage(strBackImgPath);
		return true;
	}

	bool SetBackImage(LPCTSTR lpszImgPath) {
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
			strBackImgPath = lpszImgPath;
			return true;
		}
		strBackImgPath.Empty();
		return false;
	}

	void Cleanup() {
		WriteIniInt(_T("UserSettings"), _T("ShowImGui"), show_imgui);
		WriteIniInt(_T("UserSettings"), _T("FullScreen"), fullScreen);
		WriteIniInt(_T("UserSettings"), _T("ParticleMax"), iSnowMax);
		CString str;
		str.Format(_T("%.2f"), camera.GetPosition().z);
		WriteIniString(_T("UserSettings"), _T("Zoom"), str);
		WriteIniString(_T("UserSettings"), _T("ImagePath"), strBackImgPath);

		SafeDelete(spriteBatch);
		SafeRelease(&backTexture);
	}

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

	HINSTANCE	hInstance;
	UINT		dpi;
	HWND		hWnd;
	SIZE		wndSize;
	bool		isDarkMode;
	bool		isScrnSaver;
	bool		fullScreen;
	ImVec4		clear_color;
	CString		strTitle;
	CString		strAppDir;
	CString		strAppName;
	CString		strIniFilePath;
	bool		show_imgui;
	bool		show_demo_window;
	bool		show_another_window;
	int			iSnowMax;

	SnowParticle	particle;
	D3DCamera		camera;
	SpriteBatch*	spriteBatch;
	CString			strBackImgPath;
	SIZE			backSize;
	float			backAspect;
	ID3D11ShaderResourceView*	backTexture;
};

extern app_state	AppState;
