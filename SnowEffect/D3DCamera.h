#pragma once

using namespace DirectX;

class D3DCamera
{
public:
	D3DCamera();
	D3DCamera(const D3DCamera&);
	~D3DCamera();

	void SetPosition(float, float, float);
	void SetRotation(float, float, float);

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetRotation();

	void Render();
	XMMATRIX& GetViewMatrix() noexcept {
		return m_viewMatrix;
	}

private:
	float m_positionX, m_positionY, m_positionZ;
	float m_rotationX, m_rotationY, m_rotationZ;
	XMMATRIX m_viewMatrix;
};
