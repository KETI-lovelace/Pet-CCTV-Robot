# 라즈베리파이 내 프로그램 상세 설명

## 프로그램 세부 구성

1. [위험 구역 설정](https://github.com/KETI-lovelace/Pet-CCTV-Robot/tree/master/raspberrypi#1-%EC%9C%84%ED%97%98-%EA%B5%AC%EC%97%AD-%EC%84%A4%EC%A0%95)
2. [모터 회전](https://github.com/KETI-lovelace/Pet-CCTV-Robot/tree/master/raspberrypi#2-%EB%AA%A8%ED%84%B0-%EA%B5%AC%EB%8F%99)
3. [반려동물 디텍팅](https://github.com/KETI-lovelace/Pet-CCTV-Robot/tree/master/raspberrypi#3-%EB%B0%98%EB%A0%A4%EB%8F%99%EB%AC%BC-%EB%94%94%ED%85%8D%ED%8C%85)
4. [소리 송출](https://github.com/KETI-lovelace/Pet-CCTV-Robot/tree/master/raspberrypi#4-%EC%86%8C%EB%A6%AC-%EC%86%A1%EC%B6%9C)

## 1. 위험 구역 설정

<img src="./assets/danger_check.gif" width="500">

### 코드부분
```python
while True:
    danger_frame = videostream.read()
    cv2.rectangle(danger_frame,(440,370),(540,410),(255,255,255),-1)
    cv2.putText(danger_frame,'NEXT'.format(frame_rate_calc),(450,400),cv2.FONT_HERSHEY_SIMPLEX,1,(50,50,255),2,cv2.LINE_AA)
    cv2.namedWindow('set_danger',cv2.WND_PROP_FULLSCREEN)
    cv2.setWindowProperty('set_danger',cv2.WND_PROP_FULLSCREEN,cv2.WINDOW_FULLSCREEN)
    cv2.imshow('set_danger', danger_frame)
    cv2.setMouseCallback('set_danger', mouse_callback)
    if(next_btn >  2): # 모든 위험구역 설정 완료
        next_btn = 2
        break
    k = cv2.waitKey(1)
    if k == 'd':
        break
cv2.destroyAllWindows()
```

### 상세 설명
- 좌측에서부터 카메라로부터 촬영되는 화면을 스크린을 통해 볼 수 있다.
- ``mousecallback`` 함수를 이용해 원하는 위험구역을 설정을 완료하고 ``NEXT`` 버튼을 누른다.
- 모터가 움직이며 우측,정면에서도 동일한 과정을 거친다.


## 2. 모터 구동

### 코드 부분

```python
# next_btn = 0 : 0 angle , = 1 : 90 angle , 2 = 180 angle

# motor angle change
pi = pigpio.pi()
pi.set_servo_pulsewidth(servoPin, 0) # turn off 18 pin connected servo motor.
pi.set_servo_pulsewidth(servoPin,800 * next_btn + 700)

# automatically change servomotor angle
while True:
    count += 1
    if( count > 180 ) :
        count = 0
        pi.set_servo_pulsewidth(servoPin,800 * next_btn + 700)
        if(is180):
            curr_btn = next_btn
            next_btn -= 1
        else:
            curr_btn = next_btn
            next_btn += 1
        if(next_btn == 2 or next_btn  == 0):
            is180 = not is180

...

```
### 상세 설명

- ``pigpio`` 모듈 사용
    - 원래는 ``ChangeDutyCycle`` 를 이용해 모터 각도를 전환시켰으나 떨림이 심해서, ``pigpio``선택
    - 설치 방법은 아래와 같다.

    ```bash
    $sudo apt-get update
    $sudo apt-get install pigpio python-pigpio python3-pigpio
    ```
    - 코드 작성 방법은 아래와 같다.

    ```python
    import pigpio

    from time import sleep
    
    pi = pigpio.pi() #먼저 사용할 pigpio.pi를 매칭해줍니다.
    while True:
        pi.set_servo_pulsewidth(18, 0) # 18번 채널에연결된 서보모터를 꺼줍니다. 
        sleep(1)
        pi.set_servo_pulsewidth(18, 700) # 18번채널에 연결된 서보모터를 0도로 이동
        sleep(1)
        pi.set_servo_pulsewidth(18, 1500) # 가운데로 이동 90도
        sleep(1)
        pi.set_servo_pulsewidth(18, 2300) # 180도 끝으로 이동. 
        sleep(1)
    ```
    - 실행 방법은 아래와 같다.

    ```bash
    $sudo pigpiod # pigpiod 실행
    $sudo python3 [파일 명]
    $sudo killall pigpiod # pigpiod 종료
    ```
- ``next_btn`` 이 0,1,2 일 때, 각각 0도, 90도 180도 표지를 의미한다.
- ``count``가 180 이상이되면, 모터 각도를 0도 -> 90도 -> 180도 -> 90도 -> 0도 순서대로 전환하는것을 반복한다.

## 3. 반려동물 디텍팅

<img src="./assets/ai_check.gif" width="500">

- 라즈베리파이 반려동물 디텍팅에는 다음의 [오픈소스](https://github.com/EdjeElectronics/TensorFlow-Lite-Object-Detection-on-Android-and-Raspberry-Pi/blob/master/Raspberry_Pi_Guide.md)를 이용했다.

## 4. 소리 송출

### 코드 부분

```python
pygame.mixer.init()
music_file = "Dontgo.wav"
pygame.mixer.music.load(music_file)
pygame.mixer.music.play()
clock=pygame.time.Clock()
while pygame.mixer.music.get_busy():
    clock.tick(5)
```

### 상세 설명

- ``pygame`` 모듈 이용

## 5. 안드로이드 앱
Firebase로 안드로이드 앱에 알림을 보내는 부분이다.
