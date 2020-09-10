#-*- coding:utf-8 -*-
#한글때문에 python3으로 실행
#그러나 지원안되는 라이브러리 때문에 다시 python2로 변경 멘붕!

import json
import requests
#import pyrebase #python3 only
from firebase import firebase
from  pyfcm import FCMNotification #python2 only
#from pusher import Pusher #python2 only
import sys


#한글에러 해결
reload(sys)
sys.setdefaultencoding('utf8')


cSky = "1"
grade = "2"
myvalue = "3"
cTemp = "4"
cWind = "5"


####FCM 알림 보내기(이거 쓰레드로 처리해야 하나? 일단 이렇게 하자.)#### 
#3. 먼저 firebase database에서 저장된 token을 모두 가져와야 한다.
firebase = firebase.FirebaseApplication("https://soboro-6d610.firebaseio.com/", None)
result = firebase.get("/users", None)
listToken = []
for k, v in result.items():
    listToken.append(v["token"])
#    break
#print(listToken)
#k ,v = result.items()
#listToken.append(v["token"])


#4.sending message with pyfcm
push_service = FCMNotification(api_key="AAAACXndXcs:APA91bETtBU2UsGeSIToJYp4SFfx0VimIQthlDvZDQD5MmDAD0dCIqbl2JOevt0O0thLAJAGNb_rxkQzeuUYF51CkqO8O0Q_HVtP84TrrTnjE8nnuLJOLXHAHdsUepMfKH2szyw8YklQ")

#data_message
data_message = {
	"message_body":"날씨: "+cSky+", 미세먼지: "+grade+"("+myvalue+")",
	"하늘":cSky,
	"온도":cTemp,
	"풍속":cWind
}

# Send to multiple devices by passing a list of ids.
registration_ids = listToken #["token1", "token2", ....]
message_title = "Soboro"
message_body = "Hurry up! Your pet is danger! Click this alarm!"
message_sound = "default"
#display message
result = push_service.notify_multiple_devices(registration_ids=registration_ids, message_title=message_title, message_body=message_body)
#data_message payload
#result = push_service.notify_multiple_devices(registration_ids=registration_ids, message_title=message_title, message_body=message_body, data_message=data_message)
#data_message payload, without message_body #- 이렇게 data_message만 보내면 앱이 백그라운드일때도 앱에서 알림을 핸들링 한다.

result = push_service.notify_multiple_devices(registration_ids=registration_ids, data_message=data_message)

#result = push_service.single_device_data_message(registration_id=registration_ids[0], data_message=data_message)




# print result
