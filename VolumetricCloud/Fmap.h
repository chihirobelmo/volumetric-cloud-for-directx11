#pragma once

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

	int windHeading_; // Global wind heading. 
	float windSpeed_; // Global wind speed.

	int stratusAltFair_; // Altitude of fair weather stratus clouds.
	int stratusAltIncl_; // Altitude of inclined stratus clouds.

	int contrailLayer_[4]; // Contrail layer data.

	std::vector<std::vector<FmapCell>> cells_; // [X][Y]
};