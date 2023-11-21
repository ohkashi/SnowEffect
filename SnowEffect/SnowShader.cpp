#include "pch.h"
#include "SnowShader.h"
#include <fstream>


SnowShader::SnowShader() : m_vertexShader(NULL), m_pixelShader(NULL), m_layout(NULL), m_matrixBuffer(NULL), m_sampleState(NULL)
{
}

SnowShader::~SnowShader()
{
}

bool SnowShader::Init(HWND hWnd)
{
	const char* vertex_shader =
		"cbuffer MatrixBuffer {\n\
			matrix worldMatrix;\n\
			matrix viewMatrix;\n\
			matrix projectionMatrix;\n\
		};\n\
		struct VertexInputType {\n\
			float4 position : POSITION;\n\
			float2 tex : TEXCOORD0;\n"
#ifdef _INCLUDE_VERTEX_COLOR
		"	float4 color : COLOR;\n"
#endif
		"};\n\
		struct PixelInputType {\n\
			float4 position : SV_POSITION;\n\
			float2 tex : TEXCOORD0;\n"
#ifdef _INCLUDE_VERTEX_COLOR
		"	float4 color : COLOR;\n"
#endif
		"};\n\
		PixelInputType ParticleVertexShader(VertexInputType input) {\n\
			PixelInputType output;\n\
			// Change the position vector to be 4 units for proper matrix calculations.\n\
			input.position.w = 1.0f;\n\
			// Calculate the position of the vertex against the world, view, and projection matrices.\n\
			output.position = mul(input.position, worldMatrix);\n\
			output.position = mul(output.position, viewMatrix);\n\
			output.position = mul(output.position, projectionMatrix);\n\
			// Store the texture coordinates for the pixel shader.\n\
			output.tex = input.tex;\n"
#ifdef _INCLUDE_VERTEX_COLOR
		"	// Store the particle color for the pixel shader.\n\
			output.color = input.color;\n"
#endif
		"	return output;\n\
		}\n";

	const char* pixel_shader =
		"Texture2D shaderTexture;\n\
		SamplerState SampleType;\n\n\
		\
		struct PixelInputType {\n\
			float4 position : SV_POSITION;\n\
			float2 tex : TEXCOORD0;\n"
#ifdef _INCLUDE_VERTEX_COLOR
		"	float4 color : COLOR;\n"
#endif
		"};\n\
		float4 ParticlePixelShader(PixelInputType input) : SV_TARGET {\n"
#ifdef _INCLUDE_VERTEX_COLOR
		"	float4 textureColor;\n\
			float4 finalColor;\n\
			// Sample the pixel color from the texture using the sampler at this texture coordinate location.\n\
			textureColor = shaderTexture.Sample(SampleType, input.tex);\n\
			// Combine the texture color and the particle color to get the final color result.\n\
			finalColor = textureColor * input.color;\n\
			return finalColor;\n"
#else
		"	return shaderTexture.Sample(SampleType, input.tex);\n"
#endif
		"}\n";

	// Initialize the vertex and pixel shaders.
	return InitShader(hWnd, vertex_shader, strlen(vertex_shader), pixel_shader, strlen(pixel_shader));
}

void SnowShader::Shutdown()
{
	// Shutdown the vertex and pixel shaders as well as the related objects.
	ShutdownShader();

	return;
}

bool SnowShader::Render(int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix, 
						XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture)
{
	// Set the shader parameters that it will use for rendering.
	if (!m_matrixBuffer || !SetShaderParameters(worldMatrix, viewMatrix, projectionMatrix, texture))
		return false;

	// Now render the prepared buffers with the shader.
	RenderShader(indexCount);
	return true;
}

bool SnowShader::InitShader(HWND hWnd, const char* vsData, size_t vsSize, const char* psData, size_t psSize)
{
	HRESULT result;
	ID3DBlob* errorMessage;
	ID3DBlob* vertexShaderBuffer;
	ID3DBlob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[3];
	unsigned int numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;
	D3D11_SAMPLER_DESC samplerDesc;

	// Initialize the pointers this function will use to null.
	errorMessage = 0;
	vertexShaderBuffer = 0;
	pixelShaderBuffer = 0;

	LPCWSTR vsFilename = L"particle.vs";
	LPCWSTR psFilename = L"particle.ps";

	// Compile the vertex shader code.
	result = D3DCompile2(vsData, vsSize, "vertex_shader", NULL, NULL, "ParticleVertexShader", "vs_5_0",
						 D3D10_SHADER_ENABLE_STRICTNESS, 0, 0, NULL, 0, &vertexShaderBuffer, &errorMessage);
	if (FAILED(result)) {
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage) {
			OutputShaderErrorMessage(errorMessage, hWnd, vsFilename);
		} else {
			// If there was nothing in the error message then it simply could not find the shader file itself.
			MessageBox(hWnd, vsFilename, L"Missing Shader File", MB_OK);
		}

		return false;
	}

	// Compile the pixel shader code.
	result = D3DCompile2(psData, psSize, "pixel_shader", NULL, NULL, "ParticlePixelShader", "ps_5_0",
						 D3D10_SHADER_ENABLE_STRICTNESS, 0, 0, NULL, 0, &pixelShaderBuffer, &errorMessage);
	if (FAILED(result)) {
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage) {
			OutputShaderErrorMessage(errorMessage, hWnd, psFilename);
		} else {
			// If there was  nothing in the error message then it simply could not find the file itself.
			MessageBox(hWnd, psFilename, L"Missing Shader File", MB_OK);
		}

		return false;
	}

	// Create the vertex shader from the buffer.
	result = D3D::Device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader);
	if (FAILED(result)) {
		return false;
	}

	// Create the pixel shader from the buffer.
	result = D3D::Device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader);
	if (FAILED(result)) {
		return false;
	}

	// Create the vertex input layout description.
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	polygonLayout[2].SemanticName = "COLOR";
	polygonLayout[2].SemanticIndex = 0;
	polygonLayout[2].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[2].InputSlot = 0;
	polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[2].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);
#ifndef _INCLUDE_VERTEX_COLOR
	--numElements;
#endif

	// Create the vertex input layout.
	result = D3D::Device->CreateInputLayout(polygonLayout, numElements,
											vertexShaderBuffer->GetBufferPointer(),
											vertexShaderBuffer->GetBufferSize(), &m_layout);
	if (FAILED(result))
		return false;

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = nullptr;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = nullptr;

	// Setup the description of the dynamic matrix constant buffer that is in the vertex shader.
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = D3D::Device->CreateBuffer(&matrixBufferDesc, NULL, &m_matrixBuffer);
	if (FAILED(result))
		return false;

	// Create a texture sampler state description.
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = D3D::Device->CreateSamplerState(&samplerDesc, &m_sampleState);
	return SUCCEEDED(result);
}

void SnowShader::ShutdownShader()
{
	// Release the sampler state.
	SafeRelease(&m_sampleState);

	// Release the matrix constant buffer.
	SafeRelease(&m_matrixBuffer);

	// Release the layout.
	SafeRelease(&m_layout);

	// Release the pixel shader.
	SafeRelease(&m_pixelShader);

	// Release the vertex shader.
	SafeRelease(&m_vertexShader);
}

void SnowShader::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, LPCWSTR shaderFilename)
{
	std::ofstream fout;

	// Get a pointer to the error message text buffer.
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	int bufferSize = (int)errorMessage->GetBufferSize();

	// Open a file to write the error message to.
	fout.open("shader-error.txt");

	// Write out the error message.
	for (int i = 0; i < bufferSize; i++) {
		fout << compileErrors[i];
	}
	// Close the file.
	fout.close();

	// Release the error message.
	errorMessage->Release();
	errorMessage = nullptr;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBox(hwnd, L"Error compiling shader.  Check shader-error.txt for message.", shaderFilename, MB_ICONERROR | MB_OK);
}

bool SnowShader::SetShaderParameters(XMMATRIX worldMatrix, XMMATRIX viewMatrix,
									 XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	MatrixBufferType* dataPtr;
	unsigned int bufferNumber;

	// Transpose the matrices to prepare them for the shader.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	// Lock the constant buffer so it can be written to.
	result = D3D::DeviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
		return false;

	// Get a pointer to the data in the constant buffer.
	dataPtr = (MatrixBufferType*)mappedResource.pData;

	// Copy the matrices into the constant buffer.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Unlock the constant buffer.
	D3D::DeviceContext->Unmap(m_matrixBuffer, 0);

	// Set the position of the constant buffer in the vertex shader.
	bufferNumber = 0;

	// Now set the constant buffer in the vertex shader with the updated values.
	D3D::DeviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_matrixBuffer);

	// Set shader texture resource in the pixel shader.
	D3D::DeviceContext->PSSetShaderResources(0, 1, &texture);
	return true;
}

void SnowShader::RenderShader(int indexCount)
{
	// Set the vertex input layout.
	D3D::DeviceContext->IASetInputLayout(m_layout);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	D3D::DeviceContext->VSSetShader(m_vertexShader, NULL, 0);
	D3D::DeviceContext->PSSetShader(m_pixelShader, NULL, 0);

	// Set the sampler state in the pixel shader.
	D3D::DeviceContext->PSSetSamplers(0, 1, &m_sampleState);

	// Render the triangle.
	D3D::DeviceContext->DrawIndexed(indexCount, 0, 0);
}
