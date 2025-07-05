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

    // ���m�N���[���ϊ����v�Z (�P�x�Ɋ�Â�)
    // ITU-R BT.709 �����Ɋ�Â����d���� (��ʓI�ȋP�x�v�Z)
   float luminance = originalColor.z * 0.2126 +
                  originalColor.y * 0.7152 +
                  originalColor.x * 0.0722;  

    // �A���t�@�����͂��̂܂�
    float4 monochromeColor = float4(luminance, luminance, luminance, originalColor.a);


OutputTexture[ID.xy] = monochromeColor;
}