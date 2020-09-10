import pigpio

from time import sleep

 
pi = pigpio.pi() #먼저 사용할 pigpio.pi를 매칭해줍니다.
# while True:
pi.set_servo_pulsewidth(18, 0) # 18번 채널에연결된 서보모터를 꺼줍니다. 
sleep(1)
pi.set_servo_pulsewidth(18, 700) # 18번채널에 연결된 서보모터를 0도로 이동
sleep(1)
pi.set_servo_pulsewidth(18, 1500) # 가운데로 이동 90도
sleep(1)
pi.set_servo_pulsewidth(18, 2300) # 180도 끝으로 이동. 
sleep(1)
