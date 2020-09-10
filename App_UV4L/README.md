# AWS를 이용한 영상 스트리밍 Android App 상세 설명
라즈베리파이에서 위험 구역에 반려 동물 진입 시 사용자 앱으로 알림을 전송합니다.<br>
앱 알림은 Firebase로 구현되어 있습니다.<br>
실시간 스트리밍 기능은 AWS로 구현되어 있습니다.<br>
이 앱을 실행시키기 위한 상세 작업은 다음과 같습니다. <br>
<br>

## 라즈베리파이에서 수행해줘야 할 작업
라즈베리파이에서 AWS 스트리밍을 실행하기 위해서는 이 레포지토리의 raspberrypi 폴더를 다운받은 뒤 다음과 같은 절차를 따릅니다.<br>
```
1. 라이브러리 설치
$ sudo apt-get install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools

2. 환경변수 설정
$ export AWS_ACCESS_KEY_ID=YourAccessKeyId
$ export AWS_SECRET_ACCESS_KEY=YourSecretAccessKey

3. 종속 라이브러리 연결
$ cd /etc/ld.so.conf.d
$ sudo vim kinesis.conf
실행 파일이랑 같이 넘겨준 libKinesisProducer.so 등이 저장되어 있는 경로 입력
$ sudo ldconfig

4. 실행
$./KinesisStreaming.out <streaming name>
```
<br>

## 안드로이드 스튜디오에서 수행해줘야 할 작업
```
private String accessKeyId = "";
private String secretKey = "";
```
이부분에 AWS 계정에서 생성한 accessKeyId와 secretKey를 입력합니다.<br>
코드 상에서는 보안 문제로 빈칸 처리해두었습니다.<br>
주의!! 앱 내에 AWS 키를 넣은 채로 깃에 푸시하면 안됩니다. 보안 문제로 메일이 쇄도할겁니다.<br>
위의 라즈베리파이에서 수행해줘야 할 작업에서 4번 부분의 <streaming name>은 앱의 AWSManager의 37번째 줄의 arten을 사용하면 됩니다.<br>
