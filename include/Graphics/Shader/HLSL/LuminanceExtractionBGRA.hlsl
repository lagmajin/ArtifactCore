Texture2D<float4> InputTexture  : register(t0); // ���̓e�N�X�`�� (RGBA��, float4�`��)
RWTexture2D<float4> OutputTexture : register(u0); 

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;

    float4 srcColor = InputTexture.Load(int3(coord, 0));

    float luminance = dot(srcColor.xyz, float3(0.299, 0.587, 0.114));

    // �P�x���O���[�X�P�[���Ƃ��ď����o���i�A���t�@�͂��̂܂܁j
    OutputTexture[coord] = float4(luminance, luminance, luminance, srcColor.a);
}