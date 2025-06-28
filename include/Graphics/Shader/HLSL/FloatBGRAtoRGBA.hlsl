Texture2D<float4> InputBGRA_Texture : register(t0);

// �o�̓e�N�X�`���iRGBA���Afloat4�`���j
// �e�N�X�`���̓A���I�[�_�[�h�A�N�Z�X�r���[ (UAV) �Ƃ��ăo�C���h�����
RWTexture2D<float4> OutputRGBA_Texture : register(u0);

// �X���b�h�O���[�v�̃T�C�Y���`
// ��: X����8�X���b�h�AY����8�X���b�h�AZ����1�X���b�h
// �摜�����ł͒ʏ� Z �� 1
[numthreads(8, 8, 1)]
void main(uint3 ID : SV_DispatchThreadID) // �S�̂ł̃X���b�hID
{
    // ���̓e�N�X�`���̃T�C�Y���擾 (�s�N�Z���P��)
    uint width, height;
    InputBGRA_Texture.GetDimensions(width, height);

    // �X���b�hID���e�N�X�`���͈͓̔��ɂ��邱�Ƃ��m�F
    // �͈͊O�̃X���b�h�͏������Ȃ�
    if (ID.x >= width || ID.y >= height)
    {
        return;
    }

    // BGRA�J���[��ǂݍ���
    // color.r �� Blue, color.g �� Green, color.b �� Red, color.a �� Alpha
    float4 bgraColor = InputBGRA_Texture[ID.xy];

    // BGRA����RGBA�֕��ёւ���
    // RGBA: Red, Green, Blue, Alpha
    // bgraColor.b �� Red
    // bgraColor.g �� Green
    // bgraColor.r �� Blue
    // bgraColor.a �� Alpha
    float4 rgbaColor = float4(bgraColor.b, bgraColor.g, bgraColor.r, bgraColor.a);

    // RGBA�J���[���o�̓e�N�X�`���ɏ�������
    OutputRGBA_Texture[ID.xy] = rgbaColor;
}
