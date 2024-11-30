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
#include "includes/Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class DDSLoader {
public:
	ComPtr<ID3D11Resource> colorTEX_;
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

	static bool LoadDDS(const char* filename, DDS_HEADER& header, std::vector<char>& data) {
		std::ifstream file(filename, std::ios::binary);
		if (!file) return false;

		uint32_t magicNumber;
		file.read(reinterpret_cast<char*>(&magicNumber), sizeof(uint32_t));
		if (magicNumber != 0x20534444) return false; // "DDS "

		file.read(reinterpret_cast<char*>(&header), sizeof(DDS_HEADER));

		data.resize(header.pitchOrLinearSize);
		file.read(data.data(), data.size());

		return true;
	}

	void Load(const char* filename) {

		DDS_HEADER header;
		std::vector<char> data;
		if (LoadDDS(filename, header, data) == false) {
			std::cout << "Failed to load DDS file" << std::endl;
			return;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = header.width;
		desc.Height = header.height;
		desc.MipLevels = header.mipMapCount;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = data.data();
		initData.SysMemPitch = header.pitchOrLinearSize / header.height;

		ID3D11Texture2D* texture = nullptr;
		Renderer::device->CreateTexture2D(&desc, &initData, &texture);
		Renderer::device->CreateShaderResourceView(colorTEX_.Get(), nullptr, &colorSRV_);
	}
};