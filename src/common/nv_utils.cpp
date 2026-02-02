#include "nv_utils.h"

#include <NvBufSurface.h>
#include <nvbufsurface.h>

int NvUtils::ConvertToI420(int src_dma_fd, uint8_t *dst_addr, size_t dst_size, int width,
                           int height) {
    int dst_dma_fd;
    NvBufSurf::NvCommonAllocateParams cParams;
    cParams.width = width;
    cParams.height = height;
    cParams.layout = NVBUF_LAYOUT_PITCH;
    cParams.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
    cParams.memtag = NvBufSurfaceTag_CAMERA;
    cParams.memType = NVBUF_MEM_SURFACE_ARRAY;
    int ret = NvBufSurf::NvAllocate(&cParams, 1, &dst_dma_fd);
    if (ret < 0) {
        return ret;
    }

    NvBufSurf::NvCommonTransformParams transform_params;
    memset(&transform_params, 0, sizeof(transform_params));
    transform_params.src_top = 0;
    transform_params.src_left = 0;
    transform_params.src_width = width;
    transform_params.src_height = height;
    transform_params.dst_top = 0;
    transform_params.dst_left = 0;
    transform_params.dst_width = width;
    transform_params.dst_height = height;
    transform_params.flag = NVBUFSURF_TRANSFORM_FILTER;
    transform_params.flip = NvBufSurfTransform_None;
    transform_params.filter = NvBufSurfTransformInter_Algo3;

    ret = NvBufSurf::NvTransform(&transform_params, src_dma_fd, dst_dma_fd);
    if (ret < 0) {
        NvBufSurf::NvDestroy(dst_dma_fd);
        return ret;
    }

    ret = ReadDmaBuffer(dst_dma_fd, dst_addr, dst_size);

    NvBufSurf::NvDestroy(dst_dma_fd);

    return ret;
}

int NvUtils::ReadDmaBuffer(int src_dma_fd, uint8_t *dst_addr, size_t dst_size) {
    if (src_dma_fd <= 0)
        return -1;

    int ret = -1;

    NvBufSurface *nvbuf_surf = 0;
    ret = NvBufSurfaceFromFd(src_dma_fd, (void **)(&nvbuf_surf));
    if (ret != 0) {
        return -1;
    }

    int offset = 0;

    for (int plane = 0; plane < nvbuf_surf->surfaceList->planeParams.num_planes; ++plane) {
        NvBufSurfaceMap(nvbuf_surf, 0, plane, NVBUF_MAP_READ);
        NvBufSurfaceSyncForCpu(nvbuf_surf, 0, plane);

        uint8_t *src_addr = static_cast<uint8_t *>(nvbuf_surf->surfaceList->mappedAddr.addr[plane]);
        int row_size = nvbuf_surf->surfaceList->planeParams.width[plane] *
                       nvbuf_surf->surfaceList->planeParams.bytesPerPix[plane];

        for (uint row = 0; row < nvbuf_surf->surfaceList->planeParams.height[plane]; ++row) {
            memcpy(dst_addr + offset,
                   src_addr + row * nvbuf_surf->surfaceList->planeParams.pitch[plane], row_size);
            offset += row_size;
        }

        NvBufSurfaceSyncForDevice(nvbuf_surf, 0, plane);
        ret = NvBufSurfaceUnMap(nvbuf_surf, 0, plane);
        if (ret < 0) {
            printf("Error while Unmapping buffer\n");
            return ret;
        }
    }

    return 0;
}
