"""
Camera Stream to Virtual V4L2 Device
------------------------------------
This script captures images from the Raspberry Pi camera and streams them 
to a virtual V4L2 loopback device using OpenCV.

Usage:
    1. Install required dependencies:
        pip install opencv-python picamera2
    
    2. Load v4l2loopback module (if not already loaded):
        sudo modprobe v4l2loopback devices=1 video_nr=16 card_label=ProcessedCam max_buffers=4 exclusive_caps=1
    
    3. Run the script:
        python virtual_cam.py

    4. Test the video output:
        /path/to/pi-webrtc --camera=v4l2:16 --width=1920 --height=1080 ...   # View the processed feed by WebRTC
        ffplay /dev/video16                                                  # View the processed feed by ffplay

Requirements:
    - Raspberry Pi with Camera Module
    - v4l2loopback kernel module installed
"""

import os
import cv2
import time
import fcntl
import v4l2
import logging
from picamera2 import Picamera2, MappedArray

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)


class VirtualCameraStreamer:
    def __init__(self, video_nr, camera_id=0, width=1920, height=1080):
        self.width = width
        self.height = height
        self.camera_id = camera_id
        self.virtual_camera = f"/dev/video{video_nr}"
        self.fd = None
        self.picam2 = None

        self._initialize_camera()
        self._initialize_virtual_device()

    def _initialize_camera(self):
        self.picam2 = Picamera2(self.camera_id)
        config = self.picam2.create_still_configuration(
            main={"size": (self.width, self.height)}, queue=False
        )
        self.picam2.configure(config)

    def _initialize_virtual_device(self):
        if not os.path.exists(self.virtual_camera):
            logging.warning(f"Device is not existing: {self.virtual_camera}")
            return

        self.fd = os.open(self.virtual_camera, os.O_RDWR)
        format = v4l2.v4l2_format()
        format.type = v4l2.V4L2_BUF_TYPE_VIDEO_OUTPUT
        format.fmt.pix.width = self.width
        format.fmt.pix.height = self.height
        format.fmt.pix.pixelformat = v4l2.V4L2_PIX_FMT_YUV420
        format.fmt.pix.field = v4l2.V4L2_FIELD_NONE
        format.fmt.pix.bytesperline = self.width
        format.fmt.pix.sizeimage = self.width * self.height
        fcntl.ioctl(self.fd, v4l2.VIDIOC_S_FMT, format)
        logging.info(f"Set camera: {self.virtual_camera}")

    def _process_frame(self, request):
        timestamp = time.strftime("%Y-%m-%d %X")
        with MappedArray(request, "main") as m:
            frame = m.array
            cv2.putText(
                frame, timestamp, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2
            )
            yuv_frame = cv2.cvtColor(frame, cv2.COLOR_RGB2YUV_I420)

            try:
                os.write(self.fd, yuv_frame.tobytes())
            except (BrokenPipeError, OSError) as e:
                logging.error(f"Failed to write images: {e}")
                self.stop()

    def start(self):
        if not self.fd:
            logging.error("Cannot start streaming without virtual device.")
            return

        self.picam2.pre_callback = self._process_frame
        self.picam2.start()
        logging.info(f"Start streaming to {self.virtual_camera}...")

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logging.info("Received KeyboardInterrupt.")
        finally:
            self.stop()

    def stop(self):
        if self.picam2:
            self.picam2.stop()
        if self.fd:
            os.close(self.fd)
        logging.info("Streaming stopped.")


if __name__ == "__main__":
    streamer = VirtualCameraStreamer(16, camera_id=1)
    streamer.start()
