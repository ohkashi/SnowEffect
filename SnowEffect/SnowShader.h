#pragma once

using namespace DirectX;

class SnowShader
{
private:
	struct MatrixBufferType {
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;
	};

public:
	SnowShader();
	~SnowShader();

	bool Init(HWND hWnd);
	void Shutdown();
	bool Render(int, XMMATRIX, XMMATRIX, XMMATRIX, ID3D11ShaderResourceView*);

protected:
	SnowShader(const SnowShader&);
	bool InitShader(HWND hWnd, const char* vsData, size_t vsSize, const char* psData, size_t psSize);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob*, HWND, LPCWSTR);

	bool SetShaderParameters(XMMATRIX, XMMATRIX, XMMATRIX, ID3D11ShaderResourceView*);
	void RenderShader(int);

private:
	ID3D11VertexShader*	m_vertexShader;
	ID3D11PixelShader*	m_pixelShader;
	ID3D11InputLayout*	m_layout;
	ID3D11Buffer*		m_matrixBuffer;
	ID3D11SamplerState*	m_sampleState;
};
