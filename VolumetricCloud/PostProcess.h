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
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	ComPtr<ID3D11SamplerState> sampler;

	// Render to texture resources
	ComPtr<ID3D11Texture2D> tex;
	ComPtr<ID3D11RenderTargetView> rtv;
	ComPtr<ID3D11ShaderResourceView> srv;

	PostProcess() {};
	~PostProcess() {};

	void CreatePostProcessResources(const std::wstring& fileName, const std::string& entryPointVS, const std::string& entryPointPS);
	void CreateRenderTexture(UINT width, UINT height);
	void Draw(UINT width, UINT height, ID3D11ShaderResourceView* const* ppShaderResourceViews, UINT numBuffers, ID3D11Buffer* const* ppConstantBuffers);

};