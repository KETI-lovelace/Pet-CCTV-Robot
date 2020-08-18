import serial
ser = serial.Serial('/dev/ttyACM0',115200)
ser.write(bytes('1',encoding='ascii'))
