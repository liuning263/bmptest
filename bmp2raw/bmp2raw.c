#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

int main() {
    int fd = open("input.bmp", O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    struct stat sb;
    fstat(fd, &sb);

    void *data = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Error mapping file");
        return 1;
    }

    BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)data;
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)((char *)data + sizeof(BITMAPFILEHEADER));

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
