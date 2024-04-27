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
    
parsing_err:
    fclose(file);
open_config_file_err:
    debug_print(DEBUG, "dev_resolution.width:%d\tdev_resolution.height:%d\t\n",dev_resolution.width,dev_resolution.height);
    return dev_resolution;
}

int process_command_line_arguments(int argc, char *argv[],char **bmp_file_name,int *rotation)
{
    
    int ret = 1;
    if(NULL == bmp_file_name || NULL == rotation)
    {
        goto cmd_line_parameter_err;
    }
    int opt = 0;
    const char *optstring = "d";
    /*-d参数表示打印DEBUG日志*/
    while ((opt = getopt(argc, argv, optstring)) != -1) 
    {
        switch (opt) {
            case 'd':
                debug_level++;
                break;
            default:
                debug_print(ERROR, "Command line parameter error\n");
                ret = 1;
                goto cmd_line_parameter_err;
        }
    }

    int extra_arg_index = optind;
    /*判断是否为两个参数*/
    if(argc == extra_arg_index + 2)
    {
        *rotation = atoi(argv[extra_arg_index + 1]);
    }
    else
    {
        debug_print(ERROR, "Command line parameter error\n");
        ret = 1;
        goto cmd_line_parameter_err;
    }

    if(NULL != argv[extra_arg_index] && strlen(argv[extra_arg_index]) > 0)
    {
        *bmp_file_name = argv[extra_arg_index];
    }
    else
    {
        debug_print(ERROR, "Please enter the BMP file name\n");
        ret = 1;
        goto file_name_err;
    }
    debug_print(DEBUG, "bmp_file_name:%s\trotation:%d\t\n",*bmp_file_name,*rotation);
    ret = 0;
file_name_err:
cmd_line_parameter_err:
    return ret;
}
int main(int argc, char *argv[]) 
{
    int ret = 1;
    int color_palette_color_number = 0;
    int rows_data_size = 0;
    char *bmp_file_name = NULL;
    int rotation = 0;

    /*读取配置文件*/
    /*如果能读取到设备分辨率，只计算屏幕大小数据*/
    resolution device_resolution = read_resolution_from_config();

    /*读取命令行*/
    ret = process_command_line_arguments(argc,argv,&bmp_file_name,&rotation);
    if(ret != 0)
    {
        goto cmd_line_parameter_err;
    }

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
    /*计算调色盘大小*/
    int color_palette_size = bfh->bfOffBits - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER);
    if(color_palette_size> 0)
    {
        color_palette_color_number = color_palette_size / PALETTE_EACH_COLOR_OCCUPIES_SIZE;
    }
    else
    {
        color_palette_color_number = 0;
    }

    uint32_t *color_palette_date = (uint32_t *)((uint8_t *)data + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
    uint8_t *bitmap_data = (uint8_t *)(color_palette_date + color_palette_color_number);

    printf("color_palette_date: %x\n", *((unsigned int *)color_palette_date));
    printf("bitmap_data: %x\n", *((unsigned int *)bitmap_data));

    debug_print(INFO, "Image bit depth:%d\tbiWidth:%d\tbiHeight:%d\tbiSizeImage:%d\t\n",bih->biBitCount,bih->biWidth,bih->biHeight,bih->biSizeImage);
    debug_print(DEBUG, "color_palette_color_number:%d\t\n",color_palette_color_number);



    /*读取数据写入raw文件*/
    int row = 0;
    int col = 0;
    int image_row_height = abs(bih->biHeight);//BMP图片的行高可能为负数，负数代表图像是倒向的，负数代表图像是正向的。

    /*如果图片超过设备大小，只转换设备分辨率的图片*/
    row = (image_row_height > device_resolution.height) ? device_resolution.height : image_row_height;
    col = (bih->biWidth > device_resolution.width) ? device_resolution.width : bih->biWidth;

    debug_print(INFO, "row:%d\tcol:%d\t\n",row,col);

    if(bih->biWidth > 0)
    {
        rows_data_size = bih->biSizeImage / bih->biHeight;
    }
    else
    {
        ret = 1;
        debug_print(ERROR, "Rows data size err\n");
        goto rows_data_size_err;
    }

    /*打开输出raw data的文件*/
    FILE *raw_file = fopen("output_rotated.raw", "wb");
    if (NULL == raw_file) {
        debug_print(ERROR, "Error opening output rotated file\n");
        goto open_output_rotated_file_err;
    }


    /*读取颜色数据*/
    debug_print(DEBUG, "rows_data_size:%d\t\n",rows_data_size);
    int i = 0;
    while(i < row)
    {
        
        
        int j = 0;
        while(j < col)
        {
            
            uint32_t color_palette_index = 0;
            uint32_t write_data = 0;
            int ros_destIndex = 0;
            int col_destIndex = 0;

            /*计算旋转的偏移量*/
            uint32_t *color_palette_color_data = NULL;
            switch (rotation)
            {
                case 90:
                    ros_destIndex = row - j - 1;
                    col_destIndex = i;
                    break;
                case 180:
                    ros_destIndex = row - i - 1;
                    col_destIndex = col - j - 1;
                    break;
                case 270:
                    ros_destIndex = j;
                    col_destIndex = col - i - 1;
                    break;
                default :
                    ros_destIndex = i;
                    col_destIndex = j;
                    break;
            }
            debug_print(DEBUG, "ros_destIndex:%d\tcol_destIndex:%d\t\n",ros_destIndex,col_destIndex);

            /*计算偏移到哪一行，信息头中的高度如果是负数说明图片是倒向，需要从最后一行开始读*/
            int row_offset = (bih->biHeight > 0) ? bih->biHeight - ros_destIndex - 1 : ros_destIndex;
            uint8_t *offset_data = (uint8_t *)(bitmap_data + (rows_data_size * row_offset));

            debug_print(DEBUG, "row_offset:%d\toffset_data:%x\t\n",row_offset,*((unsigned int *)offset_data));

            /*计算偏移到哪一列*/
            switch(bih->biBitCount)
            {
                case 1 :
                    /*先计算出在哪个字节字节中，然后右移到该位，*/
                    color_palette_index = ((*(offset_data + col_destIndex / 8)) >> (8 - col_destIndex - 1)) & 0x01;
                    color_palette_color_data = (uint32_t*)(color_palette_date + color_palette_index);
                    debug_print(DEBUG, "color_palette_index:%d\tcolor_palette_color_data:%08x\t\n",color_palette_index,*((uint32_t *)color_palette_color_data));
                    break;
                case 4 :
                    /*先计算出在哪个字节字节中，然后找出具体索引号*/
                    color_palette_index = ((*(offset_data + col_destIndex / 2)) >> ((col_destIndex % 2 == 0) ? 4: 0)) & 0x0f;
                    color_palette_color_data = (uint32_t*)(color_palette_date + color_palette_index);
                    debug_print(DEBUG, "color_palette_index:%d\tcolor_palette_color_data:%08x\t\n",color_palette_index,*((uint32_t *)color_palette_color_data));
                break;
                case 8 :
                    /*直接读取该字节的值*/
                    color_palette_index = *(offset_data + col_destIndex);
                    color_palette_color_data = (uint32_t*)(color_palette_date + color_palette_index);
                    debug_print(DEBUG, "color_palette_index:%d\tcolor_palette_color_data:%08x\t\n",color_palette_index,*((uint32_t *)color_palette_color_data));
                break;
                case 16 :
                    /*判断是否使用mask方式*/
                    uint32_t color_data = (uint32_t)(((*(offset_data + col_destIndex * 2)) << 8) | *(offset_data + col_destIndex * 2 + 1));
                    if(color_palette_size == MASK_COLOR_PALETTE_SIZE)
                    {
                        uint32_t r_mask_data = (((uint32_t)*(color_palette_date )) << 8) | *(offset_data + 1);
                        uint32_t g_mask_data = (((uint32_t)*(color_palette_date +  2)) << 8) | *(offset_data + 3);
                        uint32_t b_mask_data = (((uint32_t)*(color_palette_date +  4)) << 8) | *(offset_data + 5);

                        uint8_t r_data = (color_data & r_mask_data) >> 8;
                        uint8_t g_data = (color_data & g_mask_data) >> 3;
                        uint8_t b_data = (color_data & b_mask_data) << 3;

                        write_data = (0xFF << 24) | (r_data << 16) | (g_data << 8) | b_data;

                    }
                    else
                    {
                        uint8_t r_data = (color_data >> 11) & 0x1F;
                        uint8_t g_data  = (color_data >> 5) & 0x3F;
                        uint8_t b_data = color_data & 0x1F; 

                        /*补偿精度*/
                        r_data = (r_data * 255) / 31;
                        g_data = (g_data * 255) / 63;
                        b_data = (b_data * 255) / 31;

                        write_data = (0xFF << 24) | (r_data << 16) | (g_data << 8) | b_data;
                    }
                break;
                case 24 :
                    write_data = (uint32_t)(0xff000000 | ((*(offset_data + col_destIndex * 3)) << 16) | ((*(offset_data + col_destIndex * 3 + 1)) << 8) | *(offset_data + col_destIndex * 3 + 2));
                break;
                case 32 :
                    write_data = (uint32_t)(((*(offset_data + col_destIndex * 4)) << 24) | *(offset_data + col_destIndex * 4 + 1) | *(offset_data + col_destIndex * 4 + 2) | *(offset_data + col_destIndex * 4 + 3));
                break;
                default:
                break;
            }
            
            if(NULL != color_palette_color_data)
            {
                /*将透明度通道设置为0xff，写入文件中*/
                write_data = *color_palette_color_data | 0xff000000;
                debug_print(DEBUG, "write_data:%08x\t\n",write_data);
                
            }
            fwrite(&write_data,sizeof(uint32_t), 1, raw_file);
            j++;
        }
        debug_print(DEBUG, "One line write completed\n");
        i++;
    }
    
    ret = 0;


    fclose(raw_file);
open_output_rotated_file_err:
    munmap(data, file_stat.st_size);
rows_data_size_err:
mmap_err:
file_header_magic_err:
fstat_file_err:
    close(fd);
open_bmp_file_err:
cmd_line_parameter_err:
    return ret;
}
