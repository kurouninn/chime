#include <wiringPi.h>

// LED ピン - wiringPi ピン 0 は BCM_GPIO 17 です。
//wiringPiSetupSys で初期化する場合は、BCM 番号付けを使用する必要があります
//別のピン番号を選択する場合は、BCM 番号付けを使用してください。また
//プロパティ ページで、[ビルド イベント]、[リモートのビルド後イベント] の順に選択し、 コマンドを更新してください
//これは、wiringPiSetupSys のセットアップに対して gpio エクスポートを使用します
#define	LED	17

int main(void)
{
	wiringPiSetupSys();

	pinMode(LED, OUTPUT);

	while (true)
	{
		digitalWrite(LED, HIGH);  //オン
		delay(500); //ミリ秒
		digitalWrite(LED, LOW);	  //オフ
		delay(500);
	}
	return 0;
}