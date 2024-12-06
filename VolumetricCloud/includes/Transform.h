#pragma once

#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>

#include "Renderer.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Transform {
public:

    struct SRTBuffer {
        XMMATRIX scale;
        XMMATRIX roation;
        XMMATRIX translation;
        XMMATRIX SRT;
    };

    ComPtr<ID3D11Buffer> buffer_;

    Transform() {
        scale = XMMatrixIdentity();
        rotation = XMMatrixIdentity();
        translation = XMMatrixIdentity();
    }

    void SetScale(float x, float y, float z) {
        scale = XMMatrixScaling(x, y, z);
    }

    void SetRotation(float pitch, float yaw, float roll) {
        rotation = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    }

    void SetTranslation(float x, float y, float z) {
        translation = XMMatrixTranslation(x, y, z);
    }

    XMMATRIX GetSRTMatrix() const {
        return scale * rotation * translation;
    }

    void CreateBuffer() {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(SRTBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA srtInitData = {};
        srtInitData.pSysMem = &bd;

        HRESULT hr = Renderer::device->CreateBuffer(&bd, &srtInitData, &buffer_);
        if (FAILED(hr))
            return;
    }

	void UpdateBuffer() {
        SRTBuffer bf;
        bf.scale = XMMatrixTranspose(scale);
		bf.roation = XMMatrixTranspose(rotation);
		bf.translation = XMMatrixTranspose(translation);
		bf.SRT = XMMatrixTranspose(GetSRTMatrix());

		Renderer::context->UpdateSubresource(buffer_.Get(), 0, nullptr, &bf, 0, 0);
    }

private:
    XMMATRIX scale;
    XMMATRIX rotation;
    XMMATRIX translation;
};

// Example usage
/*
Transform transform;
transform.SetScale(1.0f, 1.0f, 1.0f);
transform.SetRotation(0.0f, XM_PI / 4.0f, 0.0f);
transform.SetTranslation(0.0f, 0.0f, 5.0f);

XMMATRIX srtMatrix = transform.GetSRTMatrix();
*/