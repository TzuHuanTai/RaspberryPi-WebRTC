#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <third_party/openh264/src/codec/api/wels/codec_api.h>

/*
transcode output.h264 stream to mp4.
`ffmpeg -f h264 -i /home/pi/IoT/RaspberryPi_WebRTC/build/output.h264 -c:v copy output.mp4`
*/

int main() {
    ISVCEncoder *encoder = nullptr;
    int rv = WelsCreateSVCEncoder(&encoder);
    if (rv != 0) {
        std::cerr << "Failed to create OpenH264 encoder." << std::endl;
        return -1;
    }

    int width = 640;
    int height = 480;
    int total_num = 100;

    SEncParamBase param;
    std::memset(&param, 0, sizeof(SEncParamBase));
    param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    param.fMaxFrameRate = 30.0;
    param.iPicWidth = width;
    param.iPicHeight = height;
    param.iTargetBitrate = 1000000; // 1Mbps
    rv = encoder->Initialize(&param);
    if (rv != 0) {
        std::cerr << "Failed to initialize OpenH264 encoder." << std::endl;
        return -1;
    }

    std::ifstream yuvFile("test.yuv", std::ios::in | std::ios::binary);
    if (!yuvFile.is_open()) {
        std::cerr << "Failed to open YUV file." << std::endl;
        return -1;
    }

    int ySize = width * height;
    int uvSize = (width / 2) * (height / 2);
    int frameSize = ySize + 2 * uvSize;
    std::vector<unsigned char> yuvBuffer(frameSize);
    unsigned char *yData = yuvBuffer.data();
    unsigned char *uData = yData + ySize;
    unsigned char *vData = uData + uvSize;

    yuvFile.read(reinterpret_cast<char *>(yuvBuffer.data()), frameSize);

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));
    SSourcePicture pic;
    memset(&pic, 0, sizeof(SSourcePicture));
    pic.iPicWidth = width;
    pic.iPicHeight = height;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = width;
    pic.iStride[1] = width / 2;
    pic.iStride[2] = width / 2;
    pic.pData[0] = yData;
    pic.pData[1] = uData;
    pic.pData[2] = vData;

    std::ofstream h264File("output.h264", std::ios::binary);
    if (!h264File.is_open()) {
        std::cerr << "Failed to open output H.264 file." << std::endl;
        return -1;
    }

    for (int num = 0; num < total_num; num++) {
        rv = encoder->EncodeFrame(&pic, &info);

        if (info.eFrameType != videoFrameTypeSkip) {
            // output bitstream
            for (int i = 0; i < info.iLayerNum; i++) {
                SLayerBSInfo *layer = &info.sLayerInfo[i];
                int iLayerSize = 0;
                int iNalIdx = layer->iNalCount - 1;
                do {
                    iLayerSize += layer->pNalLengthInByte[iNalIdx];
                    --iNalIdx;
                } while (iNalIdx >= 0);
                h264File.write(reinterpret_cast<char *>(layer->pBsBuf), iLayerSize);
            }
        }
    }

    encoder->Uninitialize();
    WelsDestroySVCEncoder(encoder);

    yuvFile.close();
    h264File.close();

    std::cout << "Encoding finished successfully." << std::endl;
    return 0;
}
