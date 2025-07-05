Texture2D<float4> InputTexture  : register(t0); // ���̓e�N�X�`��
RWTexture2D<float4> OutputTexture : register(u0); // �o�̓e�N�X�`��

[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID)
{
    uint width, height;
    InputTexture.GetDimensions(width, height);

    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    float4 originalColor = InputTexture[ID.xy];

    // R��B�̃`���l�����X���b�v
    // ��: originalColor.rgba = (B, G, R, A) �̏ꍇ�AprocessedColor.rgba = (R, G, B, A) �ɂȂ�
    // ��: originalColor.rgba = (R, G, B, A) �̏ꍇ�AprocessedColor.rgba = (B, G, R, A) �ɂȂ�
    float4 swappedColor = float4(originalColor.b, originalColor.g, originalColor.r, originalColor.a);

    OutputTexture[ID.xy] = swappedColor;
}