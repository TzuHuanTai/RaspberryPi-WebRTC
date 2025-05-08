"""
Camera Stream to Virtual V4L2 Device
------------------------------------
This script captures images from the Raspberry Pi camera and streams them 
to a virtual V4L2 loopback device using OpenCV. 

It allows real-time object detection using YOLO while keeping the original 
camera feed accessible without delay.

Usage:
    1. Install required dependencies:
        pip install opencv-python ultralytics
    
    2. Load v4l2loopback module:
        sudo modprobe v4l2loopback devices=2 video_nr=16,17 card_label=RelayCam,YoloCam max_buffers=4 exclusive_caps=1,1
    
    3. Start `virtual_cam.py` first:
        python virtual_cam.py
    
    4. Run `yolo_cam.py` to apply YOLO detection:
        python yolo_cam.py

    5. Test the video output:
        /path/to/pi-webrtc --camera=v4l2:16 --width=1920 --height=1080 ...   # View original camera feed
        /path/to/pi-webrtc --camera=v4l2:17 --width=1920 --height=1080 ...   # View YOLO-processed feed

Requirements:
    - Raspberry Pi with Camera Module
    - v4l2loopback kernel module installed
    - Python packages: opencv-python, ultralytics
"""

import os
import cv2
import time
import fcntl
import v4l2
import logging
from ultralytics import YOLO

model = YOLO("/home/pi/yolo11n.pt")
class_names = model.names

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class VirtualCameraStreamer:
    def __init__(self, input_device, output_device, width=1920, height=1080):
        self.width = width
        self.height = height
        self.input_device = input_device
        self.output_device = output_device
        self.fd = None
        self.cap = None

        self._initialize_camera()
        self._initialize_virtual_device()

    def _initialize_camera(self):
        self.cap = cv2.VideoCapture(self.input_device)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        if not self.cap.isOpened():
            logging.error(f"Failed to open input camera: {self.input_device}")

    def _initialize_virtual_device(self):
        if not os.path.exists(self.output_device):
            logging.warning(f"Device does not exist: {self.output_device}")
            return

        self.fd = os.open(self.output_device, os.O_RDWR)
        format = v4l2.v4l2_format()
        format.type = v4l2.V4L2_BUF_TYPE_VIDEO_OUTPUT
        format.fmt.pix.width = self.width
        format.fmt.pix.height = self.height
        format.fmt.pix.pixelformat = v4l2.V4L2_PIX_FMT_YUV420
        format.fmt.pix.field = v4l2.V4L2_FIELD_NONE
        format.fmt.pix.bytesperline = self.width
        format.fmt.pix.sizeimage = self.width * self.height
        fcntl.ioctl(self.fd, v4l2.VIDIOC_S_FMT, format)
        logging.info(f"Set virtual camera: {self.output_device}")

    def _process_frame(self, frame):
        timestamp = time.strftime("%Y-%m-%d %X")
        results = model(frame)

        for result in results:
            for box in result.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                class_id = int(box.cls[0])
                conf = float(box.conf[0])
                label = f"{class_names[class_id]} {conf:.2%}"

                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                text_size, _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)
                text_w, text_h = text_size
                cv2.rectangle(
                    frame, (x1, y1 - text_h - 5), (x1 + text_w, y1), (0, 255, 0), -1
                )
                cv2.putText(
                    frame,
                    label,
                    (x1, y1 - 5),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (0, 0, 0),
                    2,
                )
                cv2.putText(
                    frame,
                    timestamp,
                    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    1,
                    (0, 255, 0),
                    2,
                )

        yuv_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2YUV_I420)
        try:
            os.write(self.fd, yuv_frame.tobytes())
        except (BrokenPipeError, OSError) as e:
            logging.error(f"Failed to write images: {e}")
            self.stop()

    def start(self):
        if not self.fd or not self.cap.isOpened():
            logging.error("Cannot start streaming without input or virtual device.")
            return

        logging.info(
            f"Start streaming from {self.input_device} to {self.output_device}..."
        )
        try:
            while True:
                ret, frame = self.cap.read()
                if not ret:
                    logging.error("Failed to read frame from camera.")
                    break
                self._process_frame(frame)
                time.sleep(0.033)  # Approx 30 FPS
        except KeyboardInterrupt:
            logging.info("Received KeyboardInterrupt.")
        finally:
            self.stop()

    def stop(self):
        if self.cap:
            self.cap.release()
        if self.fd:
            os.close(self.fd)
        logging.info("Streaming stopped.")


if __name__ == "__main__":
    streamer = VirtualCameraStreamer("/dev/video16", "/dev/video17")
    streamer.start()
