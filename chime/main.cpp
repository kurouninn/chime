#include <wiringPi.h>

// LED �s�� - wiringPi �s�� 0 �� BCM_GPIO 17 �ł��B
//wiringPiSetupSys �ŏ���������ꍇ�́ABCM �ԍ��t�����g�p����K�v������܂�
//�ʂ̃s���ԍ���I������ꍇ�́ABCM �ԍ��t�����g�p���Ă��������B�܂�
//�v���p�e�B �y�[�W�ŁA[�r���h �C�x���g]�A[�����[�g�̃r���h��C�x���g] �̏��ɑI�����A �R�}���h���X�V���Ă�������
//����́AwiringPiSetupSys �̃Z�b�g�A�b�v�ɑ΂��� gpio �G�N�X�|�[�g���g�p���܂�
#define	LED	17

int main(void)
{
	wiringPiSetupSys();

	pinMode(LED, OUTPUT);

	while (true)
	{
		digitalWrite(LED, HIGH);  //�I��
		delay(500); //�~���b
		digitalWrite(LED, LOW);	  //�I�t
		delay(500);
	}
	return 0;
}