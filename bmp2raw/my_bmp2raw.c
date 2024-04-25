#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "my_bmp2raw.h"


/*默认输出info、warning、error级别日志。添加-d选项后可以输出debug级别*/
static int debug_level = 2;


#define debug_print(level, fmt, ...) \
    do { \
        if (debug_level >= level) { \
            fprintf(stderr, "[%s][%s %d] ", \
                level == DEBUG ? "DEBUG" : \
                level == INFO ? "INFO" : \
                level == WARNING ? "WARNING" : \
                level == ERROR ? "ERROR" : "UNKNOWN", \
                __FILE__,__LINE__); \
            fprintf(stderr, fmt, ##__VA_ARGS__); \
        } \
    } while (0)


resolution read_resolution_from_config(void) {
    resolution dev_resolution = {0, 0};
    FILE *file = fopen(CONFIG_FILE_NAME, "r");

    if (NULL == file) {
        debug_print(ERROR, "Error opening config file\n");
        goto open_config_file_err;
    }

    if (fscanf(file, "width=%d height=%d", &dev_resolution.width, &dev_resolution.height) != 2) {
        debug_print(ERROR, "Error reading resolution from file.\n");
        goto parsing_err;
    }
    
    debug_print(DEBUG, "dev_resolution.width:%d\tdev_resolution.height:%d\t\n",dev_resolution.width,dev_resolution.height);


parsing_err:
    fclose(file);
open_config_file_err:
    return dev_resolution;
}

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

int main(int argc, char *argv[]) 
{
    int ret = 1;
    int opt = 0;
    int color_palette_flag = 0;
    int color_palette_color_number = 0;

    char *bmp_file_name = NULL;
    const char *optstring = "d";


    /*读取配置文件*/
    /*如果能读取到设备分辨率，只计算屏幕大小数据*/
    resolution device_resolution = read_resolution_from_config();
    if(device_resolution.height > 0 && device_resolution.width > 0)
    {
        debug_print(INFO, "Device Resolution: %d x %d\n", device_resolution.width, device_resolution.height);
    }


    /*-d参数表示打印DEBUG日志*/
    while ((opt = getopt(argc, argv, optstring)) != -1) 
    {
        switch (opt) {
            case 'd':
                debug_level++;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                ret = 1;
                goto getopt_err;
        }
    }

    int extra_arg_index = optind;

    if(NULL != argv[extra_arg_index] && strlen(argv[extra_arg_index]) > 0)
    {
        bmp_file_name = argv[extra_arg_index];
    }
    else
    {
        debug_print(ERROR, "Please enter the BMP file name\n");
        ret = 1;
        goto file_name_err;
    }
    debug_print(DEBUG, "bmp_file_name:%s\t\n",bmp_file_name);

    

    /*打开文件*/
    int fd = open(bmp_file_name, O_RDONLY);
    if (fd == -1)
    {
        debug_print(ERROR, "Error opening file\n");
        ret = 1;
        goto open_bmp_file_err;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat)== -1) 
    {
        debug_print(ERROR, "fstat file err.\n");
        ret = 1;
        goto fstat_file_err;
    }

    /*初步验证合法性*/
    /*判断文件前两个字节是否是 0x424d。文件是否大于54字节*/
    char buffer[CHECK_BM_BUFFER_SIZE + 1] = "";
    read(fd, buffer, CHECK_BM_BUFFER_SIZE);
    debug_print(DEBUG, "buffer:%s\tst_size:%ld\t\n",buffer,file_stat.st_size);
    if (strncmp(buffer,CHECK_BM_STR,CHECK_BM_BUFFER_SIZE) != 0 && file_stat.st_size >= (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) )
    {
        debug_print(ERROR, "File does not start with 'BM'.\n");
        ret = 1;
        goto file_header_magic_err;
    }

    /*使用mmap考虑到可能会有比较大的文件进行转换，在旋转时直接读取数据偏移 向raw文件中写，这样不需要额外的内存，而且不需要大小端转换*/
    /*将文件映射到内存中*/
    void *data = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        debug_print(ERROR, "Error mapping file\n");
        ret = 1;
        goto mmap_err;
    }

    BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)data;
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)((char *)data + sizeof(BITMAPFILEHEADER));
    
    

    debug_print(DEBUG, "bfOffBits:%d\t\n",bfh->bfOffBits);
    /*判断是否有调色盘*/
    int color_palette_size = bfh->bfOffBits - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER);
    if(color_palette_size> 0)
    {
        color_palette_flag = 1;
        color_palette_color_number = color_palette_size / PALETTE_EACH_COLOR_OCCUPIES_SIZE;
    }
    else
    {
        color_palette_flag = 0;
    }

    uint8_t *color_palette_date = (uint8_t *)((char *)data + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
    uint8_t *bitmap_data = (uint8_t *)(color_palette_date + color_palette_size);

    debug_print(INFO, "Image bit depth:%d\tbiWidth:%d\tbiHeight:%d\tbiSizeImage:%d\t\n",bih->biBitCount,bih->biWidth,bih->biHeight,bih->biSizeImage);
    debug_print(DEBUG, "color_palette_flag:%d\tcolor_palette_color_number:%d\t\n",color_palette_flag,color_palette_color_number);
    /*读取文件位深度*/
    /*位深度 影响数据区大小，颜色映射位置*/
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
    debug_print(DEBUG, "padding:%d\tdataSize:%d\t\n",padding,dataSize);
    
#if 0
    uint8_t *rotatedData = (uint8_t *)malloc(dataSize * sizeof(uint8_t));

    // Rotate image 90 degrees
    rotateImage((uint8_t *)((char *)data + bfh->bfOffBits), rotatedData, bih->biWidth, bih->biHeight, 90, bih->biBitCount);

    FILE *rawFile = fopen("output_rotated.raw", "wb");  
    fwrite(rotatedData, 1, dataSize, rawFile);

    fclose(rawFile);
    free(rotatedData);
    munmap(data, file_stat.st_size);
#endif

    ret = 0;
mmap_err:
file_header_magic_err:
fstat_file_err:
    close(fd);
open_bmp_file_err:
file_name_err:
getopt_err:
    return ret;
}
