# AWS를 이용한 영상 스트리밍 상세 설명
이 파일을 영상 스트리밍을 할 라즈베리파이에 다운받고 AWS 스트리밍을 위해 다음과 같은 작업을 해줍니다.<br>
<br>

## 라즈베리파이에서 수행해줘야 할 작업
라즈베리파이에서 AWS 스트리밍을 실행하기 위해서는 이 레포지토리의 raspberrypi_AWS 폴더를 다운받은 뒤 다음과 같은 절차를 따릅니다.<br>
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
<streaming name>은 App_AWS에서 설정되어있는 arten입니다. 변경하려면 직접 안드로이드 스튜디오에서 코드를 수정하세요.
