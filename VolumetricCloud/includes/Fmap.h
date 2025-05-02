#pragma once

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

class FmapCell {
public:

	int   basicCondition_;
	float pressure_;
	float temperature_;
	float windSpeed_[10];
	float windHeading_[10];
	float cumulusAlt_;
	int	  cumulusDensity_;
	float cumulusSize_;
	int   hasTowerCumulus_;
	int   hasShowerCumulus_;
	float fogEndBelowLayerMapData_; // Fog layer boundary
	float fogLayerAlt_;
};

/// <summary>
/// FMAP (FalconBMS weather map data) file reader.
/// </summary>
class Fmap {
public:
	Fmap(std::string fname);
	~Fmap() {}

	int X_, Y_;

	int windHeading_; // Global wind heading. 
	float windSpeed_; // Global wind speed.

	int stratusAltFair_; // Altitude of fair weather stratus clouds.
	int stratusAltIncl_; // Altitude of inclined stratus clouds.

	int contrailLayer_[4]; // Contrail layer data.

	std::vector<std::vector<FmapCell>> cells_; // [X][Y]

	ComPtr<ID3D11Texture2D> colorTEX_;
	ComPtr<ID3D11ShaderResourceView> colorSRV_;

	bool CreateTexture2DFromData();
	void UpdateTextureData();

	bool CreateTexture2DFromDataWithComputeShader();
};