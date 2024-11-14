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
	inline static ComPtr<ID3D11VertexShader> vs;
	inline static ComPtr<ID3D11PixelShader> ps;
	inline static ComPtr<ID3D11SamplerState> sampler;

	// Render to texture resources
	inline static ComPtr<ID3D11Texture2D> tex;
	inline static ComPtr<ID3D11RenderTargetView> rtv;
	inline static ComPtr<ID3D11ShaderResourceView> srv;

	static void CreatePostProcessResources();
	static void CreateRenderTexture(UINT width, UINT height);

};