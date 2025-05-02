#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "../includes/Fmap.h"
#include <wtypes.h>
#include <functional>
#include <d3d11.h>
#include <wrl/client.h>
#include "../includes/Renderer.h"

Fmap::Fmap(std::string fname) {

	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, fname.c_str(), "rb");

	X_ = 59;
	Y_ = 59;

	// initialize cells_
	for (int i = (X_ - 1); i > -1; i--) {
		std::vector<FmapCell> YCells;
		for (int j = 0; j < Y_; j++) {
			FmapCell cell;
			YCells.push_back(cell);
		}
		cells_.push_back(YCells);
	}

	if (pFile == nullptr) { return; }

	DWORD ver = 0;
	fread(&ver, sizeof(ver), 1, pFile);

	fread(&X_, sizeof(X_), 1, pFile);
	fread(&Y_, sizeof(Y_), 1, pFile);

	fread(&windHeading_, sizeof(windHeading_), 1, pFile);
	fread(&windSpeed_, sizeof(windSpeed_), 1, pFile);

	fread(&stratusAltFair_, sizeof(stratusAltFair_), 1, pFile);
	fread(&stratusAltIncl_, sizeof(stratusAltIncl_), 1, pFile);

	for (int i = 0; i < 4; i++) {
		fread(&contrailLayer_[i], sizeof(contrailLayer_[i]), 1, pFile);
	}

	// lambda function to loop through all cells
	auto forCellXY = [&](std::function<void(int i, int j)> func) {
		for (int i = (X_ - 1); i > -1; i--) {
			for (int j = 0; j < Y_; j++) {
				func(i, j);
			}
		}
	};

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].basicCondition_, sizeof(cells_[i][j].basicCondition_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].pressure_, sizeof(cells_[i][j].pressure_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].temperature_, sizeof(cells_[i][j].temperature_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		for (int k = 0; k < 10; k++) {
			fread(&cells_[i][j].windSpeed_[k], sizeof(cells_[i][j].windSpeed_[k]), 1, pFile);
		}
	});

	forCellXY([&](int i, int j) {
		for (int k = 0; k < 10; k++) {
			fread(&cells_[i][j].windHeading_[k], sizeof(cells_[i][j].windHeading_[k]), 1, pFile);
			cells_[i][j].windHeading_[k] = (cells_[i][j].windHeading_[k] - 180.f);
			if (cells_[i][j].windHeading_[k] < 0) {
				cells_[i][j].windHeading_[k] += 360.f;
			}
		}
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].cumulusAlt_, sizeof(cells_[i][j].cumulusAlt_), 1, pFile);
		cells_[i][j].cumulusAlt_ *= -1.0f;
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].cumulusDensity_, sizeof(cells_[i][j].cumulusDensity_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].cumulusSize_, sizeof(cells_[i][j].cumulusSize_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].hasTowerCumulus_, sizeof(cells_[i][j].hasTowerCumulus_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		if (ver >= 7) {
			fread(&cells_[i][j].hasShowerCumulus_, sizeof(cells_[i][j].hasShowerCumulus_), 1, pFile);
		}
		else {
			cells_[i][j].hasShowerCumulus_ = 1;
		}
	});

	forCellXY([&](int i, int j) {
		float temp;
		fread(&temp, sizeof(cells_[i][j].fogEndBelowLayerMapData_), 1, pFile);
		cells_[i][j].fogEndBelowLayerMapData_ = temp * /*FEET_PER_KM*/3279.98f;
	});

	if (ver < 8) {
		forCellXY([&](int i, int j) {
			cells_[i][j].fogLayerAlt_ = cells_[i][j].cumulusAlt_;
		});
	}
	else {
		forCellXY([&](int i, int j) {
			fread(&cells_[i][j].fogLayerAlt_, sizeof(cells_[i][j].fogLayerAlt_), 1, pFile);
			cells_[i][j].fogLayerAlt_ *= -1.0f;
		});
	}

	fclose(pFile);
};

bool Fmap::CreateTexture2DFromData() {
	// Convert double array to RGBA format
	std::vector<uint16_t> pixelData(X_ * Y_ * 4);
	for (int y = 0; y < Y_; y++) {
		for (int x = 0; x < X_; x++) {
			int idx = (y * X_ + x) * 4;
			pixelData[idx + 0] = static_cast<uint16_t>(+((cells_[y][x].cumulusDensity_-1)/12.0) * 65535.0); // R
			pixelData[idx + 1] = static_cast<uint16_t>(+(cells_[y][x].cumulusSize_/5) * 65535.0); // G
			pixelData[idx + 2] = static_cast<uint16_t>(-cells_[y][x].cumulusAlt_); // B
			pixelData[idx + 3] = 65535.0;   // A
		}
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = X_;
	desc.Height = Y_;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16G16B16A16_UINT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT; //D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0; //D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixelData.data();
	initData.SysMemPitch = X_ * 4 * 2; // 4 channels (RGBA) * 2 bytes per channel

	HRESULT hr = Renderer::device->CreateTexture2D(&desc, &initData, &colorTEX_);
	if (FAILED(hr)) return false;

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;

	hr = Renderer::device->CreateShaderResourceView(colorTEX_.Get(), &srvDesc, &colorSRV_);
	return SUCCEEDED(hr);
}

void Fmap::UpdateTextureData() {
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	Renderer::context->Map(colorTEX_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

	uint16_t* texPtr = (uint16_t*)mappedResource.pData;
	for (int y = 0; y < Y_; y++) {
		for (int x = 0; x < X_; x++) {
			int idx = x * 4;
			texPtr[idx + 0] = static_cast<uint16_t>(+((cells_[y][x].cumulusDensity_ - 1) / 12.0) * 65535.0); // R
			texPtr[idx + 1] = static_cast<uint16_t>(+(cells_[y][x].cumulusSize_ / 5) * 65535.0); // G
			texPtr[idx + 2] = static_cast<uint16_t>(-cells_[y][x].cumulusAlt_); // B
			texPtr[idx + 3] = 65535.0;   // A
		}
		texPtr += mappedResource.RowPitch;
	}

	Renderer::context->Unmap(colorTEX_.Get(), 0);
}

namespace {

bool CompileComputeShaderFromSource(const std::wstring& source, const std::string& entryPoint, ID3D11Device* device, ID3D11ComputeShader** computeShader) {
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3DCompileFromFile(
		source.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.c_str(),
		"cs_5_0",
		D3DCOMPILE_ENABLE_STRICTNESS,
		0,
		&shaderBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		if (errorBlob) {
			MessageBoxA(nullptr, (char*)errorBlob->GetBufferPointer(), "Shader Compilation Error", MB_OK | MB_ICONERROR);
		}
		return false;
	}

	hr = device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, computeShader);
	return SUCCEEDED(hr);
}

} // namespace

bool Fmap::CreateTexture2DFromDataWithComputeShader() {

	ComPtr<ID3D11ComputeShader> computeShader_;

	if (!computeShader_) {
		if (!(CompileComputeShaderFromSource(L"shaders/FMAP.hlsl", "main", Renderer::device.Get(), &computeShader_))) {
			return false;
		}
	}

	// 1. Upload input data to a StructuredBuffer
	std::vector<XMFLOAT4> pixelData(X_ * Y_);
	for (int y = 0; y < Y_; y++) {
		for (int x = 0; x < X_; x++) {
			int idx = y * X_ + x;
            #include <DirectXMath.h> // Add this include at the top of the file

            using namespace DirectX; // Add this to use DirectXMath types like float4

            // Replace all occurrences of "float4" with "XMFLOAT4" in the code
			pixelData[idx] = XMFLOAT4(
				((cells_[y][x].cumulusDensity_ - 1) / 12.0f),
				(cells_[y][x].cumulusSize_ / 5.0f),
				-cells_[y][x].cumulusAlt_,
				1.0f // Alpha
			);
		}
	}

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = static_cast<UINT>(sizeof(XMFLOAT4) * pixelData.size());
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixelData.data();

	ComPtr<ID3D11Buffer> inputBuffer;
	HRESULT hr = Renderer::device->CreateBuffer(&bufferDesc, &initData, &inputBuffer);
	if (FAILED(hr)) return false;

	// 2. Create an SRV for the input buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementWidth = static_cast<UINT>(pixelData.size());

	ComPtr<ID3D11ShaderResourceView> inputSRV;
	hr = Renderer::device->CreateShaderResourceView(inputBuffer.Get(), &srvDesc, &inputSRV);
	if (FAILED(hr)) return false;

	// 3. Create a texture for the output
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = X_;
	texDesc.Height = Y_;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

	hr = Renderer::device->CreateTexture2D(&texDesc, nullptr, &colorTEX_);
	if (FAILED(hr)) return false;

	// 4. Create a UAV for the output texture
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = texDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

	ComPtr<ID3D11UnorderedAccessView> outputUAV;
	hr = Renderer::device->CreateUnorderedAccessView(colorTEX_.Get(), &uavDesc, &outputUAV);
	if (FAILED(hr)) return false;

	// 5. Dispatch the Compute Shader
	Renderer::context->CSSetShaderResources(0, 1, inputSRV.GetAddressOf());
	Renderer::context->CSSetUnorderedAccessViews(0, 1, outputUAV.GetAddressOf(), nullptr);
	Renderer::context->CSSetShader(computeShader_.Get(), nullptr, 0);

	UINT threadGroupX = (X_ + 15) / 16; // 16 is the thread group size of the Compute Shader
	UINT threadGroupY = (Y_ + 15) / 16;
	Renderer::context->Dispatch(threadGroupX, threadGroupY, 1);

	// 6. Create and return the SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC outputSRVDesc = {};
	outputSRVDesc.Format = texDesc.Format;
	outputSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	outputSRVDesc.Texture2D.MipLevels = 1;

	hr = Renderer::device->CreateShaderResourceView(colorTEX_.Get(), &outputSRVDesc, &colorSRV_);

	return SUCCEEDED(hr);
}