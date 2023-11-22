#include "pch.h"
#include "SnowParticle.h"
#include "resource.h"
#ifdef USE_DIRECTX_TEX
#include <DirectXTex.h>
#else
#include <DDSTextureLoader.h>
#endif
#include <wincodec.h>
#include <algorithm>


SnowParticle::SnowParticle() : m_size(0, 0, 0), m_position(0, 0, 0), m_snowSize(0.05f), m_maxParticles(0), m_totalTime(0),
							   m_particleCount(0), m_pTexture(nullptr), m_texture_size({0}), m_vertexCount(0), m_indexCount(0),
							   m_vertices(nullptr), m_vertexBuffer(nullptr), m_indexBuffer(nullptr)
{
}

SnowParticle::~SnowParticle()
{
	Cleanup();
	FreeBuffers();
}

bool SnowParticle::Init(HWND hWnd, const XMFLOAT3& position, int numMax, float w, float h, float d) noexcept
{
	Cleanup();

	m_size.x = w;
	m_size.y = h;
	m_size.z = d;
	m_vtParticles.clear();
	m_maxParticles = max(1, numMax);
	m_particleCount = 0;
	m_totalTime = 0;
	m_position = position;

	m_vtParticles.reserve(m_maxParticles);
	m_vtParticles.resize(m_maxParticles);
	if (m_vtParticles.size() != m_maxParticles)
		return false;

	for (auto& sf : m_vtParticles)
		sf.Init(m_snowSize, m_position, m_size.x, snow::GetRandom(m_size.y), m_size.z);
	m_particleCount = m_maxParticles;

	if (!LoadTexture(NULL, MAKEINTRESOURCE(IDR_DDS_PARTICLE))) {
		MessageBox(hWnd, _T("LoadTexture() failed!"), _T("SnowParticle"), MB_ICONERROR | MB_OK);
		m_vtParticles.clear();
		return false;
	}

	if (!m_shader.Init(hWnd)) {
		MessageBox(hWnd, _T("SnowShader::Init() failed!"), _T("SnowParticle"), MB_ICONERROR | MB_OK);
	}

	return InitBuffers();
}

void SnowParticle::CreateParticle(int amount) noexcept
{
	for (int i = 0; i < amount; i++) {
		if (m_particleCount >= m_maxParticles)
			break;
		m_vtParticles[m_particleCount].Init(m_snowSize, m_position, m_size.x, m_size.y, m_size.z);
		++m_particleCount;
	}
}

void SnowParticle::Cleanup() noexcept
{
	m_vtParticles.clear();
	m_particleCount = 0;
	m_totalTime = 0;
}

bool SnowParticle::InitBuffers() noexcept
{
	// Set the maximum number of vertices in the vertex array.
	m_vertexCount = m_maxParticles * 6;

	// Set the maximum number of indices in the index array.
	m_indexCount = m_vertexCount;

	// Create the vertex array for the particles that will be rendered.
	m_vertices = new snow::VertexType[m_vertexCount];
	if (!m_vertices)
		return false;

	// Create the index array.
	ULONG* indices = new ULONG[m_indexCount];
	if (!indices) {
		delete[] m_vertices;
		m_vertices = nullptr;
		return false;
	}

	// Initialize vertex array to zeros at first.
	ZeroMemory(m_vertices, sizeof(snow::VertexType) * m_vertexCount);

	// Initialize the index array.
	for (int i = 0; i < m_indexCount; i++) {
		indices[i] = i;
	}

	// Set up the description of the dynamic vertex buffer.
	D3D11_BUFFER_DESC vertexBufferDesc;
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = sizeof(snow::VertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	D3D11_SUBRESOURCE_DATA vertexData;
	vertexData.pSysMem = m_vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now finally create the vertex buffer.
	HRESULT hr = D3D::Device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	while (SUCCEEDED(hr)) {
		// Set up the description of the static index buffer.
		D3D11_BUFFER_DESC indexBufferDesc;
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the index data.
		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = indices;
		indexData.SysMemPitch = 0;
		indexData.SysMemSlicePitch = 0;

		// Create the index buffer.
		hr = D3D::Device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
		break;
	}
	// Release the index array since it is no longer needed.
	delete[] indices;
	return SUCCEEDED(hr);
}

void SnowParticle::FreeBuffers() noexcept
{
	// Release the index buffer.
	if (m_indexBuffer) {
		m_indexBuffer->Release();
		m_indexBuffer = nullptr;
	}

	// Release the vertex buffer.
	if (m_vertexBuffer) {
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

	delete[] m_vertices;
	m_vertices = nullptr;
}

bool SnowParticle::UpdateBuffers() noexcept
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	snow::VertexType* verticesPtr;

	// Initialize vertex array to zeros at first.
	ZeroMemory(m_vertices, sizeof(snow::VertexType) * m_vertexCount);

	// Now build the vertex array from the particle list array.  Each particle is a quad made out of two triangles.
	int i = 0;
	for (const auto& sf : m_vtParticles) {
		// Bottom left.
		m_vertices[i].position = XMFLOAT3(sf.position.x - sf.size, sf.position.y - sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(0.0f, 1.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;

		// Top left.
		m_vertices[i].position = XMFLOAT3(sf.position.x - sf.size, sf.position.y + sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(0.0f, 0.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;

		// Bottom right.
		m_vertices[i].position = XMFLOAT3(sf.position.x + sf.size, sf.position.y - sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(1.0f, 1.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;

		// Bottom right.
		m_vertices[i].position = XMFLOAT3(sf.position.x + sf.size, sf.position.y - sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(1.0f, 1.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;

		// Top left.
		m_vertices[i].position = XMFLOAT3(sf.position.x - sf.size, sf.position.y + sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(0.0f, 0.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;

		// Top right.
		m_vertices[i].position = XMFLOAT3(sf.position.x + sf.size, sf.position.y + sf.size, sf.position.z);
		m_vertices[i].texture = XMFLOAT2(1.0f, 0.0f);
#ifdef _INCLUDE_VERTEX_COLOR
		m_vertices[i].color = sf.color;
#endif
		++i;
	}

	// Lock the vertex buffer.
	HRESULT hr = D3D::DeviceContext->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr))
		return false;

	// Get a pointer to the data in the vertex buffer.
	verticesPtr = (snow::VertexType*)mappedResource.pData;

	// Copy the data into the vertex buffer.
	CopyMemory(verticesPtr, (void*)m_vertices, sizeof(snow::VertexType) * m_vertexCount);

	// Unlock the vertex buffer.
	D3D::DeviceContext->Unmap(m_vertexBuffer, 0);
	return true;
}

void SnowParticle::Update(float scalar) noexcept
{
	int i, numParticles = 0;
	for (i = 0; i < m_particleCount; ) {
		auto& sf = m_vtParticles[i];
		sf.position.x += sf.velocity.x * scalar;
		sf.position.y += sf.velocity.y * scalar;
		sf.position.z += sf.velocity.z * scalar;
		if (sf.position.y > m_position.y)
			++i;
		else
			sf = m_vtParticles[--m_particleCount];
	}
	//std::sort(m_vtParticles.begin(), m_vtParticles.begin() + m_particleCount);
	m_totalTime += scalar;
	numParticles = SnowFlake::SnowUpdateAmount * (int)m_totalTime;
	m_totalTime -= 1 / SnowFlake::SnowUpdateAmount * numParticles;
	CreateParticle(numParticles);
}

void SnowParticle::Render(int cx, int cy) noexcept
{
	UpdateBuffers();

	// Set vertex buffer stride and offset.
	UINT stride = sizeof(snow::VertexType);
	UINT offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	D3D::DeviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	D3D::DeviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer.
	D3D::DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

bool SnowParticle::RenderShader(XMMATRIX& worldMatrix, XMMATRIX& viewMatrix, XMMATRIX& projectionMatrix) noexcept
{
	return m_shader.Render(m_indexCount, worldMatrix, viewMatrix, projectionMatrix, m_pTexture);
}

bool SnowParticle::LoadTexture(HMODULE hModule, LPCTSTR resName, LPCTSTR resType) noexcept
{
	// Locate the resource in the application's executable.
	HRSRC imageResHandle = FindResource(hModule, resName, resType);
	if (!imageResHandle)
		return false;

	// Load the resource to the HGLOBAL.
	HGLOBAL imageResDataHandle = LoadResource(hModule, imageResHandle);
	if (!imageResDataHandle)
		return false;

	// Lock the resource to retrieve memory pointer.
	void* pImageFile = LockResource(imageResDataHandle);
	if (!pImageFile) {
		FreeResource(imageResHandle);
		return false;
	}

	// Calculate the size.
	DWORD imageFileSize = SizeofResource(hModule, imageResHandle);

	HRESULT hr;
#ifdef USE_DIRECTX_TEX
	TexMetadata meta;
	ScratchImage image;
	hr = LoadFromDDSMemory(pImageFile, imageFileSize, DDS_FLAGS_NONE, &meta, image);
	if (SUCCEEDED(hr)) {
		hr = CreateShaderResourceView(D3D::Device, image.GetImages(), image.GetImageCount(), meta, &m_pTexture);
	}
#else
	hr = CreateDDSTextureFromMemory(D3D::Device, (const uint8_t*)pImageFile, imageFileSize, NULL, &m_pTexture);
#endif

	UnlockResource(imageResDataHandle);
	FreeResource(imageResDataHandle);
	return SUCCEEDED(hr);
}
