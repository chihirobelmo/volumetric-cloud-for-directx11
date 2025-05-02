// ComputeShader.hlsl
RWTexture2D<float4> OutputTexture : register(u0); // 書き込み用テクスチャ
StructuredBuffer<float4> InputData : register(t0); // 入力データ

[numthreads(16, 16, 1)] // スレッドグループのサイズ
void main(uint3 DTid : SV_DispatchThreadID) {

    float x;
    float y;
    OutputTexture.GetDimensions(x, y); // テクスチャのサイズを取得

    // テクスチャの範囲内か確認
    if (DTid.x >= x || DTid.y >= y) {
        return;
    }

    // 入力データをテクスチャに書き込む
    uint index = DTid.y * x + DTid.x;
    OutputTexture[DTid.xy] = InputData[index];
}