#pragma once

#include <chrono>
#include <d3d11_1.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>
#include <string>
#include <format>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

#include "Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DDSLoader {
public:
	ComPtr<ID3D11Texture2D> colorTEX_;
	ComPtr<ID3D11ShaderResourceView> colorSRV_;

	struct DDS_PIXELFORMAT {
		uint32_t size;
		uint32_t flags;
		uint32_t fourCC;
		uint32_t RGBBitCount;
		uint32_t RBitMask;
		uint32_t GBitMask;
		uint32_t BBitMask;
		uint32_t ABitMask;
	};

	struct DDS_HEADER {
		uint32_t size;
		uint32_t flags;
		uint32_t height;
		uint32_t width;
		uint32_t pitchOrLinearSize;
		uint32_t depth;
		uint32_t mipMapCount;
		uint32_t reserved1[11];
		DDS_PIXELFORMAT ddspf;
		uint32_t caps;
		uint32_t caps2;
		uint32_t caps3;
		uint32_t caps4;
		uint32_t reserved2;
	};

	struct DDS_HEADER_DXT10 {
		DXGI_FORMAT dxgiFormat;
		uint32_t resourceDimension;
		uint32_t miscFlag;
		uint32_t arraySize;
		uint32_t miscFlags2;
	};

	DDS_HEADER_DXT10 dx10Header_;

	std::wstring fileName_ = L"";

	bool LoadDDS(const std::wstring& fileName, DDS_HEADER& header, std::vector<char>& data);
	DXGI_FORMAT GetDXGIFormat(const DDS_PIXELFORMAT& ddspf, float& blockSize);
	void Load(const std::wstring& fileName);
	void LoadAgain() { Load(fileName_); }
};