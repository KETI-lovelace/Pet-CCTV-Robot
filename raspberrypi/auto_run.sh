#!/bin/bash
sudo pigpiod
for var in {1..4}
do
	sudo python3 TFLite_detection_webcam.py --modeldir=Sample_TFLite_model --edgetpu
done

sudo killall pigpiod
