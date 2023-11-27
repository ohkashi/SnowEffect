#pragma once

#include "SnowShader.h"
#include <vector>

using namespace DirectX;

namespace snow {
	template<typename T>
	constexpr T GetRandom(T val) {
		return (((float)rand() - (float)rand()) / RAND_MAX) * val;
	}

	struct VertexType {
		XMFLOAT3	position;
		XMFLOAT2	texture;
#ifdef _INCLUDE_VERTEX_COLOR
		XMFLOAT4	color;
#endif
	};
}

class SnowFlake
{
public:
	SnowFlake() : size(0), position(0, 0, 0), velocity(0, 0, 0)
#ifdef _INCLUDE_VERTEX_COLOR
		, color(1, 1, 1, 1)
#endif
	{}
	~SnowFlake() {}
	void Init(float snow_size, const XMFLOAT3& pos, float width, float height, float depth) {
		this->size = snow_size;
		this->position.x = pos.x + snow::GetRandom(width);
		this->position.y = height;
		this->position.z = pos.z + snow::GetRandom(depth);
		this->velocity.x = Direction_X + snow::GetRandom(Delta_X);
		this->velocity.y = Direction_Y + snow::GetRandom(Delta_Y);
		this->velocity.z = Direction_Z + snow::GetRandom(Delta_Z);
#ifdef _INCLUDE_VERTEX_COLOR
		this->color.x = snow::GetRandom(1.0f) + 0.5f;
		this->color.y = snow::GetRandom(1.0f) + 0.5f;
		this->color.z = snow::GetRandom(1.0f) + 0.5f;
#endif
	}
	bool operator<(const SnowFlake& rhs) {
		return (this->position.z > rhs.position.z);
	}

	constexpr static float	Direction_X = 0.0f;
	constexpr static float	Direction_Y = -0.2f;
	constexpr static float	Direction_Z = 0.0f;
	constexpr static float	Delta_X = 0.1f;
	constexpr static float	Delta_Y = 0.2f;
	constexpr static float	Delta_Z = 0.1f;
	constexpr static int	SnowUpdateAmount = 1000;

	float		size;
	XMFLOAT3	position;
	XMFLOAT3	velocity;
#ifdef _INCLUDE_VERTEX_COLOR
	XMFLOAT4	color;
#endif
};

class SnowParticle
{
public:
	SnowParticle();
	~SnowParticle();

	bool Init(HWND hWnd, const XMFLOAT3& position, int numMax, float w, float h, float d) noexcept;
	void CreateParticle(int amount) noexcept;
	void Cleanup() noexcept;

	void Update(float scalar) noexcept;
	void Render(int cx, int cy) noexcept;
	bool RenderShader(XMMATRIX& worldMatrix, XMMATRIX& viewMatrix, XMMATRIX& projectionMatrix) noexcept;

	inline XMFLOAT3& GetViewSize() { return m_size; }
	inline void SetSnowSize(float snowSize) { m_snowSize = snowSize; }

protected:
	bool InitBuffers() noexcept;
	void FreeBuffers() noexcept;
	bool UpdateBuffers() noexcept;

	bool LoadTexture(HMODULE hModule, LPCTSTR resName, LPCTSTR resType = _T("DDS")) noexcept;

private:
	XMFLOAT3	m_size;
	XMFLOAT3	m_position;
	std::vector<SnowFlake>	m_vtParticles;
	int		m_maxParticles;
	int		m_particleCount;
	float	m_snowSize;
	float	m_totalTime;
	ID3D11ShaderResourceView* m_pTexture;
	SIZE	m_texture_size;
	int		m_vertexCount, m_indexCount;
	snow::VertexType*	m_vertices;
	ID3D11Buffer*	m_vertexBuffer;
	ID3D11Buffer*	m_indexBuffer;
	SnowShader		m_shader;
};
