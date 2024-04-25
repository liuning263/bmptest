#ifndef __MY_BMP2RAW_H_
#define __MY_BMP2RAW_H_

/*debug等级*/
#define ERROR 0
#define WARNING 1
#define INFO 2
#define DEBUG 3


#define PALETTE_EACH_COLOR_OCCUPIES_SIZE 4

#define BUFFER_SIZE 1024

#define CONFIG_FILE_NAME "config.txt"

#define CHECK_BM_STR "BM"
#define CHECK_BM_BUFFER_SIZE 2

/*保存BMP文件的信息头*/
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


// 定义保存设备分辨率的结构体
typedef struct {
    int width;
    int height;
} resolution;

#endif /*__MY_BMP2RAW_H_*/