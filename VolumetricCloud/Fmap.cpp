#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "Fmap.h"
#include <wtypes.h>
#include <functional>

Fmap::Fmap(std::string fname) {

	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, fname.c_str(), "rb");

	if (pFile == nullptr) { return; }

	DWORD ver = 0;
	fread(&ver, sizeof(ver), 1, pFile);

	int X, Y;
	fread(&X, sizeof(X), 1, pFile);
	fread(&Y, sizeof(Y), 1, pFile);

	fread(&windHeading_, sizeof(windHeading_), 1, pFile);
	fread(&windSpeed_, sizeof(windSpeed_), 1, pFile);

	fread(&stratusAltFair_, sizeof(stratusAltFair_), 1, pFile);
	fread(&stratusAltIncl_, sizeof(stratusAltIncl_), 1, pFile);

	for (int i = 0; i < 4; i++) {
		fread(&contrailLayer_[i], sizeof(contrailLayer_[i]), 1, pFile);
	}

	// initialize cells_
	for (int i = (X - 1); i > -1; i--) {
		std::vector<FmapCell> YCells;
		for (int j = 0; j < Y; j++) {
			FmapCell cell;
			YCells.push_back(cell);
		}
		cells_.push_back(YCells);
	}

	// lambda function to loop through all cells
	auto forCellXY = [&](std::function<void(int i, int j)> func) {
		for (int i = (X - 1); i > -1; i--) {
			for (int j = 0; j < Y; j++) {
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
		fread(&cells_[i][j].hasShowerCumulus_, sizeof(cells_[i][j].hasShowerCumulus_), 1, pFile);
	});

	forCellXY([&](int i, int j) {
		float temp;
		fread(&temp, sizeof(cells_[i][j].fogEndBelowLayerMapData_), 1, pFile);
		cells_[i][j].fogEndBelowLayerMapData_ = temp * /*FEET_PER_KM*/3279.98f;
	});

	forCellXY([&](int i, int j) {
		fread(&cells_[i][j].fogLayerAlt_, sizeof(cells_[i][j].fogLayerAlt_), 1, pFile);
		cells_[i][j].fogLayerAlt_ *= -1.0f;
	});

	fclose(pFile);
};