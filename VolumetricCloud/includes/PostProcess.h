#pragma once

#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

#include "Camera.h"
#include "Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class PostProcess {
public:
	// Post-process resources
	ComPtr<ID3D11VertexShader> vertexShader_;
	ComPtr<ID3D11PixelShader> pixelShader_;
	ComPtr<ID3D11SamplerState> linearSampler_;
	ComPtr<ID3D11SamplerState> pixelSampler_;

	// Render to texture resources
	ComPtr<ID3D11Texture2D> texture_;
	ComPtr<ID3D11RenderTargetView> renderTargetView_;
	ComPtr<ID3D11ShaderResourceView> shaderResourceView_;

	std::wstring shaderFilePath_ = L"";
	std::string entryPointVS_ = "";
	std::string entryPointPS_ = "";

	UINT width_ = 0, height_ = 0;

	PostProcess(UINT width = 0, UINT height = 0) : width_(width), height_(height) {};
	~PostProcess() {};

	void RecompileShader();
	void CreatePostProcessResources(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
	void CreateRenderTexture(UINT width, UINT height);
	void Draw(UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, UINT numBuffers, ID3D11Buffer* const* ppConstantBuffers);
	void Draw(ID3D11RenderTargetView* pRenderTargetView, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, UINT numBuffers, ID3D11Buffer* const* ppConstantBuffers);

};