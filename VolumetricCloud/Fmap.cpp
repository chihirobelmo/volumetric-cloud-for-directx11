#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "Fmap.h"
#include <wtypes.h>
#include <functional>
#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer.h"

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
	std::vector<uint8_t> pixelData(X_ * Y_ * 4);
	for (int y = 0; y < Y_; y++) {
		for (int x = 0; x < X_; x++) {
			int idx = (y * X_ + x) * 4;
			pixelData[idx + 0] = static_cast<uint8_t>(cells_[y][x].cumulusDensity_ * 255.0); // R
			pixelData[idx + 1] = static_cast<uint8_t>(cells_[y][x].cumulusSize_ * 255.0); // G
			pixelData[idx + 2] = static_cast<uint8_t>(cells_[y][x].cumulusAlt_ * 255.0); // B
			pixelData[idx + 3] = 255;   // A
		}
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = X_;
	desc.Height = Y_;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = pixelData.data();
	initData.SysMemPitch = X_ * 4; // 4 = RGBA

	HRESULT hr = Renderer::device->CreateTexture2D(&desc, &initData, &texture_);
	if (FAILED(hr)) return false;

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	hr = Renderer::device->CreateShaderResourceView(texture_.Get(), &srvDesc, &colorSRV_);
	return SUCCEEDED(hr);
}

void Fmap::UpdateTextureData() {
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	Renderer::context->Map(texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

	uint8_t* texPtr = (uint8_t*)mappedResource.pData;
	for (int y = 0; y < Y_; y++) {
		for (int x = 0; x < X_; x++) {
			int idx = x * 4;
			texPtr[idx + 0] = static_cast<uint8_t>(cells_[y][x].cumulusDensity_ * 255.0); // R
			texPtr[idx + 1] = static_cast<uint8_t>(cells_[y][x].cumulusSize_ * 255.0); // G
			texPtr[idx + 2] = static_cast<uint8_t>(cells_[y][x].cumulusAlt_ * 255.0); // B
			texPtr[idx + 3] = 255;   // A
		}
		texPtr += mappedResource.RowPitch;
	}

	Renderer::context->Unmap(texture_.Get(), 0);
}