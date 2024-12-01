#include <algorithm>
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

#include "../includes/Renderer.h"
#include "../includes/DDSLoader.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

bool DDSLoader::LoadDDS(const std::wstring& fileName, DDS_HEADER& header, std::vector<char>& data) {
	std::ifstream file(fileName, std::ios::binary);
	if (!file) {
		std::cout << "Failed to open file" << std::endl;
		return false;
	}

	uint32_t magicNumber;
	file.read(reinterpret_cast<char*>(&magicNumber), sizeof(uint32_t));
	if (magicNumber != 0x20534444) {
		return false; // "DDS "
	}

	file.read(reinterpret_cast<char*>(&header), sizeof(DDS_HEADER));

	if (header.ddspf.fourCC == 0x30315844/*808540228*/) { // "DX10"
		file.read(reinterpret_cast<char*>(&dx10Header_), sizeof(DDS_HEADER_DXT10));
	}

	data.resize(header.pitchOrLinearSize);
	file.read(data.data(), data.size());

	return true;
}

DXGI_FORMAT DDSLoader::GetDXGIFormat(const DDS_PIXELFORMAT& ddspf, float& blockSize) {
	if (ddspf.flags & 0x4) { // DDPF_FOURCC
		// blockSize = 8 for BC1, BC4
		// blockSize = 16 for BC2, BC3, BC5, BC7
		switch (ddspf.fourCC) {
		case '1TXD': blockSize = 8; return DXGI_FORMAT_BC1_UNORM;
		case '3TXD': blockSize = 16; return DXGI_FORMAT_BC2_UNORM;
		case '5TXD': blockSize = 16; return DXGI_FORMAT_BC3_UNORM;
		case '7TXD': blockSize = 16; return DXGI_FORMAT_BC7_UNORM;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

void DDSLoader::Load(const std::wstring& fileName) {

	DDS_HEADER header;
	std::vector<char> data;
	if (LoadDDS(fileName, header, data) == false) {
		std::cout << "Failed to load DDS file" << std::endl;
		return;
	}

	float blockSize = 16;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = header.width;
	desc.Height = header.height;
	desc.MipLevels = header.mipMapCount;
	desc.ArraySize = 1;
	desc.Format = dx10Header_.dxgiFormat; // depends on the DDS file
	desc.ArraySize = dx10Header_.arraySize;
	desc.MiscFlags = dx10Header_.miscFlag;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data.data();
#ifdef NON_COMPRESSED
	initData.SysMemPitch = header.pitchOrLinearSize / header.height;
#else
	initData.SysMemPitch = ((header.width + 3) / 4) * /*blockSize*/16;
#endif
	// for mips
	std::vector<D3D11_SUBRESOURCE_DATA> subresources(header.mipMapCount);

	const char* srcData = data.data();
	for (UINT mip = 0; mip < header.mipMapCount; ++mip) {
		subresources[mip].pSysMem = srcData;
		subresources[mip].SysMemPitch = (max(1U, (header.width >> mip)) + 3) / 4 * blockSize;
		subresources[mip].SysMemSlicePitch = 0;

		srcData += subresources[mip].SysMemPitch * max(1U, header.height >> mip);
	}

	HRESULT hr = Renderer::device->CreateTexture2D(&desc, &initData, &colorTEX_);
	if (FAILED(hr)) {
		std::cerr << "Failed to create texture, HRESULT: " << std::hex << hr << std::endl;
		return;
	}
	hr = Renderer::device->CreateShaderResourceView(colorTEX_.Get(), nullptr, &colorSRV_);
	if (FAILED(hr)) {
		std::cerr << "Failed to create shader resource view, HRESULT: " << std::hex << hr << std::endl;
		return;
	}
}