Texture2D<float4> InputTexture  : register(t0); // ���̓e�N�X�`�� (RGBA��, float4�`��)
RWTexture2D<float4> OutputTexture : register(u0); // �o�̓e�N�X�`�� (RGBA��, float4�`��)

// �X���b�h�O���[�v�̃T�C�Y���`
[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID) // �S�̂ł̃X���b�hID
{
    // ���̓e�N�X�`���̃T�C�Y���擾 (�s�N�Z���P��)
    uint width, height;
    InputTexture.GetDimensions(width, height);

    // �X���b�hID���e�N�X�`���͈͓̔��ɂ��邱�Ƃ��m�F
    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    // ���̓J���[��ǂݍ��� (RGBA)
    float4 originalColor = InputTexture[ID.xy];

    // �l�K�|�W���]���v�Z
    // RGB������ 1.0 �������
    // �A���t�@ (A) �����͂��̂܂�
    float4 invertedColor = float4(1.0 - originalColor.r,
                                  1.0 - originalColor.g,
                                  1.0 - originalColor.b,
                                  originalColor.a);

    // ���ʂ��o�̓e�N�X�`���ɏ�������
    OutputTexture[ID.xy] = invertedColor;
}