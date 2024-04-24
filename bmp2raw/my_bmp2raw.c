#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "my_bmp2raw.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

void rotateImage(uint8_t *src, uint8_t *dest, int width, int height, int rotation, int bitCount) {
    int i, j;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            int srcIndex = (i * width + j) * (bitCount / 8);
            int destIndex = 0;
            switch (rotation) {
                case 90:
                    destIndex = ((width - j - 1) * height + i) * 4;
                    break;
                case 180:
                    destIndex = ((height - i - 1) * width + (width - j - 1)) * 4;
                    break;
                case 270:
                    destIndex = (j * height + (height - i - 1)) * 4;
                    break;
            }
            if (bitCount == 24) {
                dest[destIndex] = 0xFF; // Alpha channel
                dest[destIndex + 1] = src[srcIndex + 2]; // Red channel
                dest[destIndex + 2] = src[srcIndex + 1]; // Green channel
                dest[destIndex + 3] = src[srcIndex]; // Blue channel
            } else if (bitCount == 32) {
                dest[destIndex] = src[srcIndex + 3]; // Alpha channel
                dest[destIndex + 1] = src[srcIndex + 2]; // Red channel
                dest[destIndex + 2] = src[srcIndex + 1]; // Green channel
                dest[destIndex + 3] = src[srcIndex]; // Blue channel
            }
        }
    }
}

int main(int argc, char *argv[]) {

    int opt;
    int debug_mode = 0;

    int color_palette_flag = 0;
    int color_palette_color_number = 0;


    const char *optstring = "qwed";
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'd':
                debug_mode = 1;
                printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\t\n\n\n",__FILE__,__FUNCTION__,__LINE__);                
                break;
            case 'q':
                debug_mode = 1;
                printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\t\n\n\n",__FILE__,__FUNCTION__,__LINE__);                
                break;
            case 'w':
                debug_mode = 1;
                printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\t\n\n\n",__FILE__,__FUNCTION__,__LINE__);                
                break;
            case 'e':
                debug_mode = 1;
                printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\t\n\n\n",__FILE__,__FUNCTION__,__LINE__);                
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                return 1;
        }
    }

    int extra_arg_index = optind;
    printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\textra_arg_index:%d\textra_arg_index:%s\t\n\n\n",__FILE__,__FUNCTION__,__LINE__,extra_arg_index,argv[extra_arg_index]);

    printf("!!!!!!filename:%s\tfunname:%s\tlinenum:%d\t\n\n\n",__FILE__,__FUNCTION__,__LINE__);
    int i = 0;
    while(i < argc)
    {
        printf("%s\n",argv[i]);
        i++;
    }

    int fd = open("input.bmp", O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }
    /*读取配置文件*/
    /*如果能读取到设备分辨率，只计算屏幕大小数据*/

    struct stat sb;
    fstat(fd, &sb);

    /*使用mmap考虑到可能会有比较大的文件进行转换，在读取之后直接向文件中写，这样不会占用很多内存*/
    void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Error mapping file");
        return 1;
    }

    BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)data;
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)((char *)data + sizeof(BITMAPFILEHEADER));
    /*判断是否有调色盘*/
    int color_palette_size = bfh->bfSize - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER);
    if(color_palette_size> 0)
    {
        color_palette_flag = 1;
        color_palette_color_number = color_palette_size / PALETTE_SIZE;
    }
    else
    {
        color_palette_flag = 0;
    }

    /*读取文件位深度*/
    switch(bih->biBitCount)
    {
        case 1 :
        break;
        case 4 :
        break;
        case 8 :
        break;
        case 16 :
        break;
        case 24 :
        break;
        case 32 :
        break;
        default:
        break;
    }

    int padding = (4 - (bih->biWidth * (bih->biBitCount / 8)) % 4) % 4;
    int dataSize = (bih->biWidth * (bih->biBitCount / 8) + padding) * bih->biHeight;

    uint8_t *rotatedData = (uint8_t *)malloc(dataSize * sizeof(uint8_t));

    // Rotate image 90 degrees
    rotateImage((uint8_t *)((char *)data + bfh->bfOffBits), rotatedData, bih->biWidth, bih->biHeight, 90, bih->biBitCount);

    FILE *rawFile = fopen("output_rotated.raw", "wb");  
    fwrite(rotatedData, 1, dataSize, rawFile);

    fclose(rawFile);
    free(rotatedData);
    munmap(data, sb.st_size);
    close(fd);

    return 0;
}
