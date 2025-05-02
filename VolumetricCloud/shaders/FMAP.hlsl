// ComputeShader.hlsl
RWTexture2D<float4> OutputTexture : register(u0); // �������ݗp�e�N�X�`��
StructuredBuffer<float4> InputData : register(t0); // ���̓f�[�^

[numthreads(16, 16, 1)] // �X���b�h�O���[�v�̃T�C�Y
void main(uint3 DTid : SV_DispatchThreadID) {

    float x;
    float y;
    OutputTexture.GetDimensions(x, y); // �e�N�X�`���̃T�C�Y���擾

    // �e�N�X�`���͈͓̔����m�F
    if (DTid.x >= x || DTid.y >= y) {
        return;
    }

    // ���̓f�[�^���e�N�X�`���ɏ�������
    uint index = DTid.y * x + DTid.x;
    OutputTexture[DTid.xy] = InputData[index];
}