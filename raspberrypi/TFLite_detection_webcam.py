
# Import packages
import os
import argparse
import cv2
import numpy as np
import sys
import time
from threading import Thread
import importlib.util
import serial
import subprocess
from pyautogui import press,typewrite,hotkey
import pygame
import time
import RPi.GPIO as GPIO
import pigpio

# Define VideoStream class to handle streaming of video from webcam in separate processing thread
# Source - Adrian Rosebrock, PyImageSearch: https://www.pyimagesearch.com/2015/12/28/increasing-raspberry-pi-fps-with-python-and-opencv/
class VideoStream:
    """Camera object that controls video streaming from the Picamera"""
    def __init__(self,resolution=(640,480),framerate=30):
        # 640 , 480
        # Initialize the PiCamera and the camera image stream
        self.stream = cv2.VideoCapture(0)
        ret = self.stream.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
        ret = self.stream.set(3,resolution[0])
        ret = self.stream.set(4,resolution[1])
            
        # Read first frame from the stream
        (self.grabbed, self.frame) = self.stream.read()

	# Variable to control when the camera is stopped
        self.stopped = False

    def start(self):
	# Start the thread that reads frames from the video stream
        Thread(target=self.update,args=()).start()
        return self

    def update(self):
        # Keep looping indefinitely until the thread is stopped
        while True:
            # If the camera is stopped, stop the thread
            if self.stopped:
                # Close camera resources
                self.stream.release()
                return

            # Otherwise, grab the next frame from the stream
            (self.grabbed, self.frame) = self.stream.read()

    def read(self):
	# Return the most recent frame
        return self.frame

    def stop(self):
	# Indicate that the camera and thread should be stopped
        self.stopped = True
# set mouse event
mouse_event_types = { 0:"EVENT_MOUSEMOVE", 1:"EVENT_LBUTTONDOWN", 2:"EVENT_RBUTTONDOWN", 3:"EVENT_MBUTTONDOWN",
                 4:"EVENT_LBUTTONUP", 5:"EVENT_RBUTTONUP", 6:"EVENT_MBUTTONUP",
                 7:"EVENT_LBUTTONDBLCLK", 8:"EVENT_RBUTTONDBLCLK", 9:"EVENT_MBUTTONDBLCLK",
                 10:"EVENT_MOUSEWHEEL", 11:"EVENT_MOUSEHWHEEL"}

mouse_event_flags = { 0:"None", 1:"EVENT_FLAG_LBUTTON", 2:"EVENT_FLAG_RBUTTON", 4:"EVENT_FLAG_MBUTTON",
                8:"EVENT_FLAG_CTRLKEY", 9:"EVENT_FLAG_CTRLKEY + EVENT_FLAG_LBUTTON",
                10:"EVENT_FLAG_CTRLKEY + EVENT_FLAG_RBUTTON", 11:"EVENT_FLAG_CTRLKEY + EVENT_FLAG_MBUTTON",

                16:"EVENT_FLAG_SHIFTKEY", 17:"EVENT_FLAG_SHIFTKEY + EVENT_FLAG_LBUTTON",
                18:"EVENT_FLAG_SHIFTLKEY + EVENT_FLAG_RBUTTON", 19:"EVENT_FLAG_SHIFTKEY + EVENT_FLAG_MBUTTON",

                32:"EVENT_FLAG_ALTKEY", 33:"EVENT_FLAG_ALTKEY + EVENT_FLAG_LBUTTON",
                34:"EVENT_FLAG_ALTKEY + EVENT_FLAG_RBUTTON", 35:"EVENT_FLAG_ALTKEY + EVENT_FLAG_MBUTTON"}

#ser = serial.Serial('/dev/ttyACM0',115200)
#ser.write(bytes('3',encoding='ascii'))

# Define and parse input arguments
parser = argparse.ArgumentParser()
parser.add_argument('--modeldir', help='Folder the .tflite file is located in',
                    required=True)
parser.add_argument('--graph', help='Name of the .tflite file, if different than detect.tflite',
                    default='detect.tflite')
parser.add_argument('--labels', help='Name of the labelmap file, if different than labelmap.txt',
                    default='labelmap.txt')
parser.add_argument('--threshold', help='Minimum confidence threshold for displaying detected objects',
                    default=0.5)
parser.add_argument('--resolution', help='Desired webcam resolution in WxH. If the webcam does not support the resolution entered, errors may occur.',
                    default='640x480')
parser.add_argument('--edgetpu', help='Use Coral Edge TPU Accelerator to speed up detection',
                    action='store_true')

args = parser.parse_args()

MODEL_NAME = args.modeldir
GRAPH_NAME = args.graph
LABELMAP_NAME = args.labels
min_conf_threshold = float(args.threshold)
resW, resH = args.resolution.split('x')
imW, imH = int(resW), int(resH)
use_TPU = args.edgetpu

# motor variables
servoPin = 18
SERVO_MAX_DUTY = 11
SERVO_MIN_DUTY = 3


# Import TensorFlow libraries
# If tflite_runtime is installed, import interpreter from tflite_runtime, else import from regular tensorflow
# If using Coral Edge TPU, import the load_delegate library
pkg = importlib.util.find_spec('tflite_runtime')
if pkg:
    from tflite_runtime.interpreter import Interpreter
    if use_TPU:
        from tflite_runtime.interpreter import load_delegate
else:
    from tensorflow.lite.python.interpreter import Interpreter
    if use_TPU:
        from tensorflow.lite.python.interpreter import load_delegate

# If using Edge TPU, assign filename for Edge TPU model
if use_TPU:
    # If user has specified the name of the .tflite file, use that name, otherwise use default 'edgetpu.tflite'
    if (GRAPH_NAME == 'detect.tflite'):
        GRAPH_NAME = 'edgetpu.tflite'       

# Get path to current working directory
CWD_PATH = os.getcwd()

# Path to .tflite file, which contains the model that is used for object detection
PATH_TO_CKPT = os.path.join(CWD_PATH,MODEL_NAME,GRAPH_NAME)

# Path to label map file
PATH_TO_LABELS = os.path.join(CWD_PATH,MODEL_NAME,LABELMAP_NAME)

# Load the label map
with open(PATH_TO_LABELS, 'r') as f:
    labels = [line.strip() for line in f.readlines()]

# Have to do a weird fix for label map if using the COCO "starter model" from
# https://www.tensorflow.org/lite/models/object_detection/overview
# First label is '???', which has to be removed.
if labels[0] == '???':
    del(labels[0])

# Load the Tensorflow Lite model.
# If using Edge TPU, use special load_delegate argument
if use_TPU:
    interpreter = Interpreter(model_path=PATH_TO_CKPT,
                              experimental_delegates=[load_delegate('libedgetpu.so.1.0')])
    print(PATH_TO_CKPT)
else:
    interpreter = Interpreter(model_path=PATH_TO_CKPT)

interpreter.allocate_tensors()

# Get model details
input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()
height = input_details[0]['shape'][1]
width = input_details[0]['shape'][2]

floating_model = (input_details[0]['dtype'] == np.float32)

input_mean = 127.5
input_std = 127.5

# Initialize frame rate calculation
frame_rate_calc = 1
freq = cv2.getTickFrequency()

# Initialize video stream
videostream = VideoStream(resolution=(imW,imH),framerate=30).start()
time.sleep(1)
# click event
next_btn = 0
# next_btn = 0 : 0 angle , = 1 : 90 angle , 2 = 180 angle 
danger_done = 0
set_danger_box = [[],[],[]]
# motor angle change

def mouse_callback(event, x, y, flags, param):
    global danger_done, set_danger_box, next_btn
    if event == 1 :
        if(x > 440 and x < 530 and y > 350 and y < 420 ):
            next_btn += 1
            if(next_btn <= 2):
                pi.set_servo_pulsewidth(servoPin, 800 * next_btn + 700)
        else:
            set_danger_box[next_btn].append((x,y))
# motor angle change
pi = pigpio.pi()
pi.set_servo_pulsewidth(servoPin, 0) # turn off 18 connected servo motor.
time.sleep(0.2)
pi.set_servo_pulsewidth(servoPin,700)
while True:
    
    danger_frame = videostream.read()
    cv2.rectangle(danger_frame,(440,370),(540,410),(255,255,255),-1)
    cv2.putText(danger_frame,'NEXT'.format(frame_rate_calc),(450,400),cv2.FONT_HERSHEY_SIMPLEX,1,(50,50,255),2,cv2.LINE_AA)
    cv2.namedWindow('set_danger',cv2.WND_PROP_FULLSCREEN)
    cv2.setWindowProperty('set_danger',cv2.WND_PROP_FULLSCREEN,cv2.WINDOW_FULLSCREEN)
    cv2.imshow('set_danger', danger_frame)
    cv2.setMouseCallback('set_danger', mouse_callback)
    if(next_btn >  2):
        next_btn = 2
        break
    k = cv2.waitKey(1)
    if k == 'd':
        break
cv2.destroyAllWindows()
#for frame1 in camera.capture_continuous(rawCapture, format="bgr",use_video_port=True):

count = 0
take_nap = 0
is180 = True
is_first = True
curr_btn = 2
next_btn = 1
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

    # Grab frame from video stream
    frame1 = videostream.read()

    # Acquire frame and resize to expected shape [1xHxWx3]
    frame = frame1.copy()
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    frame_resized = cv2.resize(frame_rgb, (width, height))
    input_data = np.expand_dims(frame_resized, axis=0)

    # Normalize pixel values if using a floating model (i.e. if model is non-quantized)
    if floating_model:
        input_data = (np.float32(input_data) - input_mean) / input_std

    # Perform the actual detection by running the model with the image as input
    interpreter.set_tensor(input_details[0]['index'],input_data)
    interpreter.invoke()

    # Retrieve detection results
    boxes = interpreter.get_tensor(output_details[0]['index'])[0] # Bounding box coordinates of detected objects
    classes = interpreter.get_tensor(output_details[1]['index'])[0] # Class index of detected objects
    scores = interpreter.get_tensor(output_details[2]['index'])[0] # Confidence of detected objects
    #num = interpreter.get_tensor(output_details[3]['index'])[0]  # Total number of detected objects (inaccurate and not needed)
    
    # Loop over all detections and draw detection box if confidence is above minimum threshold
    for i in range(len(set_danger_box[curr_btn])):
        if(i % 2 == 1):
            cv2.rectangle(frame,set_danger_box[curr_btn][i-1],set_danger_box[curr_btn][i],(0,100,255),2)
    
    for i in range(len(scores)):
        if ((scores[i] > min_conf_threshold) and (scores[i] <= 1.0)):
            if(labels[int(classes[i])] == 'stop sign'):
                cv2.destroyAllWindows()
                videostream.stop()
                quit()
            elif(labels[int(classes[i])] == 'cat' or labels[int(classes[i])] == 'dog'):
                # Get bounding box coordinates and draw box
                # Interpreter can return coordinates that are outside of image dimensions, need to force them to be within image using max() and min()
                ymin = int(max(1,(boxes[i][0] * imH)))
                xmin = int(max(1,(boxes[i][1] * imW)))
                ymax = int(min(imH,(boxes[i][2] * imH)))
                xmax = int(min(imW,(boxes[i][3] * imW)))
            
                cv2.rectangle(frame, (xmin,ymin), (xmax,ymax), (10, 255, 0), 2)
                
                for i in range(0,len(set_danger_box[curr_btn]),2):
                    if((xmin < set_danger_box[curr_btn][i][0] < xmax and ymin < set_danger_box[curr_btn][i][1] < ymax) or (xmin < set_danger_box[curr_btn][i+1][0] < xmax and ymin < set_danger_box[curr_btn][i+1][1] < ymax) or (xmin < set_danger_box[curr_btn][i][0] < xmax and ymin < set_danger_box[curr_btn][i+1][1] < ymax) or (xmin < set_danger_box[curr_btn][i][0] < xmax and ymin < set_danger_box[curr_btn][i+1][1] < ymax)):
                        
                        take_nap += 1
                        if(take_nap > 2) :
                            pygame.mixer.init()
                            music_file = "Dontgo.wav"
                            pygame.mixer.music.load(music_file)
                            pygame.mixer.music.play()
                            clock=pygame.time.Clock()
                            while pygame.mixer.music.get_busy():
                                clock.tick(5)
                            subprocess.call(['bash', 'runappalarm.sh'])
                            count = 0
                            take_nap = 0
                        
    # All the results have been drawn on the frame, so it's time to display it.
    cv2.namedWindow('Object detector',cv2.WND_PROP_FULLSCREEN)
    cv2.setWindowProperty('Object detector',cv2.WND_PROP_FULLSCREEN,cv2.WINDOW_FULLSCREEN)
    cv2.imshow('Object detector', frame)

    # Press 'q' to quit
    if cv2.waitKey(1) == ord('q'):
        break

# Clean up
cv2.destroyAllWindows()
videostream.stop()
