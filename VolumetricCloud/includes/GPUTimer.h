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

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class GPUTimer {
private:
    ComPtr<ID3D11Query> disjointQuery_;
    ComPtr<ID3D11Query> startQuery_;
    ComPtr<ID3D11Query> endQuery_;
    ID3D11DeviceContext* context_;

public:
    void Init(ID3D11Device* device, ID3D11DeviceContext* context) {
        context_ = context;

        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        device->CreateQuery(&queryDesc, &disjointQuery_);

        queryDesc.Query = D3D11_QUERY_TIMESTAMP;
        device->CreateQuery(&queryDesc, &startQuery_);
        device->CreateQuery(&queryDesc, &endQuery_);
    }

    void Start() {
        context_->Begin(disjointQuery_.Get());
        context_->End(startQuery_.Get());
    }

    void End() {
        context_->End(endQuery_.Get());
        context_->End(disjointQuery_.Get());
    }

    double GetGPUTimeInMS() {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (context_->GetData(disjointQuery_.Get(), &disjointData, sizeof(disjointData), 0) != S_OK);

        UINT64 startTime = 0;
        while (context_->GetData(startQuery_.Get(), &startTime, sizeof(startTime), 0) != S_OK);

        UINT64 endTime = 0;
        while (context_->GetData(endQuery_.Get(), &endTime, sizeof(endTime), 0) != S_OK);

        if (disjointData.Disjoint == FALSE) {
            double frequency = static_cast<double>(disjointData.Frequency);
            double time = static_cast<double>(endTime - startTime) / frequency;
            return time * 1000.0; // Convert to milliseconds
        }
        return 0.0;
    }
};