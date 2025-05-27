#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

#pragma pack(push, 1)
struct BMPFileHeader {
    char bfType[2];
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};

struct BMPInfoHeader {
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
};
#pragma pack(pop)

void loadBMP(const string& filename, BMPFileHeader& fh, BMPInfoHeader& ih, vector<unsigned char>& data, int& rowSize) {
    ifstream in(filename, ios::binary);
    if (!in) throw runtime_error("无法打开输入文件");

    in.read((char*)&fh, sizeof(fh));
    in.read((char*)&ih, sizeof(ih));
    if (fh.bfType[0] != 'B' || fh.bfType[1] != 'M' || ih.biBitCount != 24 || ih.biCompression != 0)
        throw runtime_error("只支持 24 位未压缩 BMP");

    int width = ih.biWidth, height = abs(ih.biHeight);
    rowSize = (width * 3 + 3) & ~3;
    data.resize(rowSize * height);
    in.seekg(fh.bfOffBits);
    in.read((char*)data.data(), data.size());
    in.close();
}

void saveBMP(const string& filename, BMPFileHeader fh, BMPInfoHeader ih, const vector<unsigned char>& data, int rowSize) {
    ofstream out(filename, ios::binary);
    if (!out) throw runtime_error("无法打开输出文件");

    ih.biSizeImage = data.size();
    fh.bfSize = fh.bfOffBits + ih.biSizeImage;
    out.write((char*)&fh, sizeof(fh));
    out.write((char*)&ih, sizeof(ih));
    out.write((char*)data.data(), data.size());
    out.close();
}

void horizontalMirror(vector<unsigned char>& data, int width, int height, int rowSize) {
    for (int y = 0; y < height; ++y) {
        unsigned char* row = &data[y * rowSize];
        for (int x = 0; x < width / 2; ++x) {
            for (int c = 0; c < 3; ++c)
                swap(row[x * 3 + c], row[(width - 1 - x) * 3 + c]);
        }
    }
}

void verticalMirror(vector<unsigned char>& data, int width, int height, int rowSize) {
    for (int y = 0; y < height / 2; ++y) {
        for (int x = 0; x < rowSize; ++x)
            swap(data[y * rowSize + x], data[(height - 1 - y) * rowSize + x]);
    }
}

void scaleImage(vector<unsigned char>& src, int width, int height, int rowSize, int scalePercent,
                vector<unsigned char>& dst, int& newWidth, int& newHeight, int& newRowSize) {
    newWidth = width * scalePercent / 100;
    newHeight = height * scalePercent / 100;
    newRowSize = (newWidth * 3 + 3) & ~3;
    dst.assign(newRowSize * newHeight, 0);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = x * width / newWidth;
            int srcY = y * height / newHeight;
            unsigned char* srcPixel = &src[srcY * rowSize + srcX * 3];
            unsigned char* dstPixel = &dst[y * newRowSize + x * 3];
            memcpy(dstPixel, srcPixel, 3);
        }
    }
}

void rotateImage(vector<unsigned char>& src, int width, int height, int rowSize, double angle,
                 vector<unsigned char>& dst, int& newWidth, int& newHeight, int& newRowSize) {
    double rad = angle * M_PI / 180.0;
    double cosA = cos(rad), sinA = sin(rad);

    newWidth = abs(width * cosA) + abs(height * sinA);
    newHeight = abs(width * sinA) + abs(height * cosA);
    newRowSize = (newWidth * 3 + 3) & ~3;
    dst.assign(newRowSize * newHeight, 255);  // 白色背景

    double cx = width / 2.0, cy = height / 2.0;
    double ncx = newWidth / 2.0, ncy = newHeight / 2.0;

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            double dx = x - ncx;
            double dy = y - ncy;

            double srcX = cosA * dx + sinA * dy + cx;
            double srcY = -sinA * dx + cosA * dy + cy;

            int ix = round(srcX), iy = round(srcY);
            if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
                unsigned char* srcPixel = &src[iy * rowSize + ix * 3];
                unsigned char* dstPixel = &dst[y * newRowSize + x * 3];
                memcpy(dstPixel, srcPixel, 3);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cout << "用法:\n"
             << "./imgtool -z <比例> input.bmp output.bmp\n"
             << "./imgtool -r <角度> input.bmp output.bmp\n"
             << "./imgtool -m -h/-v input.bmp output.bmp\n";
        return 0;
    }

    string mode = argv[1];
    BMPFileHeader fh;
    BMPInfoHeader ih;
    vector<unsigned char> imageData;
    int width, height, rowSize;

    try {
        if (mode == "-z") {
            int scale = atoi(argv[2]);
            string inFile = argv[3];
            string outFile = argv[4];
            loadBMP(inFile, fh, ih, imageData, rowSize);
            width = ih.biWidth;
            height = abs(ih.biHeight);

            vector<unsigned char> newImage;
            int newWidth, newHeight, newRowSize;
            scaleImage(imageData, width, height, rowSize, scale, newImage, newWidth, newHeight, newRowSize);

            ih.biWidth = newWidth;
            ih.biHeight = ih.biHeight > 0 ? newHeight : -newHeight;
            saveBMP(outFile, fh, ih, newImage, newRowSize);
        } else if (mode == "-r") {
            double angle = atof(argv[2]);
            string inFile = argv[3];
            string outFile = argv[4];
            loadBMP(inFile, fh, ih, imageData, rowSize);
            width = ih.biWidth;
            height = abs(ih.biHeight);

            vector<unsigned char> newImage;
            int newWidth, newHeight, newRowSize;
            rotateImage(imageData, width, height, rowSize, angle, newImage, newWidth, newHeight, newRowSize);

            ih.biWidth = newWidth;
            ih.biHeight = ih.biHeight > 0 ? newHeight : -newHeight;
            saveBMP(outFile, fh, ih, newImage, newRowSize);
        } else if (mode == "-m") {
            string direction = argv[2];
            string inFile = argv[3];
            string outFile = argv[4];
            loadBMP(inFile, fh, ih, imageData, rowSize);
            width = ih.biWidth;
            height = abs(ih.biHeight);

            if (direction == "-h") horizontalMirror(imageData, width, height, rowSize);
            else if (direction == "-v") verticalMirror(imageData, width, height, rowSize);
            else throw runtime_error("无效镜像方向：使用 -h 或 -v");

            saveBMP(outFile, fh, ih, imageData, rowSize);
        } else {
            throw runtime_error("无效操作参数！");
        }

        cout << "图像处理完成。" << endl;

    } catch (exception& e) {
        cerr << "错误: " << e.what() << endl;
        return 1;
    }

    return 0;
}
