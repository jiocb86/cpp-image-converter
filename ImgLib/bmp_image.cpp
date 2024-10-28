#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>
#include <vector>
#include <optional>

using namespace std;

namespace img_lib {

    PACKED_STRUCT_BEGIN BitmapFileHeader {
        // Поля заголовка Bitmap File Header        
        std::uint16_t bm_type = 0x4D42;                              // Тип файла (BM = 0x4D42)
        uint32_t file_size;                                          // Размер файла в байтах
        uint8_t reserved[4];                                         // Зарезервированные 4 байта
        uint32_t padding;                                            // Смещение начала данных
    }
    PACKED_STRUCT_END

    PACKED_STRUCT_BEGIN BitmapInfoHeader {
        // Поля заголовка Bitmap Info Header
        std::uint32_t header_size = sizeof(BitmapInfoHeader);        // Размер заголовка
        std::int32_t img_width;                                      // Ширина изображения в пикселях
        std::int32_t img_height;                                     // Высота изображения в пикселях
        std::uint16_t planes = 1;                                    // Количество цветовых плоскостей
        std::uint16_t bits_per_pixel = 24;                           // Количество бит на пиксель (RGB, 24 бита)
        std::uint32_t compression = 0;                               // Метод сжатия (0 - без сжатия)
        std::uint32_t data_size;                                     // Размер данных изображения
        int32_t horizontal_resolution = 11811;                       // Горизонтальное разрешение (пикселей на метр)
        int32_t vertical_resolution = 11811;                         // Вертикальное разрешение (пикселей на метр)
        int32_t used_colors_count = 0;                               // Количество используемых цветов
        int32_t significant_colors_count = 0x1000000;                // Количество значимых цветов
    }
    PACKED_STRUCT_END

    static int GetBMPStride(int width) {
        return 4 * ((width * 3 + 3) / 4);
    }

    // Сохранение изображения в формате BMP
    bool SaveBMP(const Path &file, const Image &image) {
        // Открытие файла
        ofstream ofs(file, ios::binary);
        if (!ofs) {
            return false; 
        }
        const int width = image.GetWidth();                           // Получаем ширину изображения
        const int height = image.GetHeight();                         // Получаем высоту изображения
        const int bmp_stride = GetBMPStride(width);                   // Вычисляем отступ по ширине
        BitmapFileHeader bitmap_file_header;
        bitmap_file_header.padding = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
        bitmap_file_header.file_size =  bitmap_file_header.padding + bmp_stride * height;  // Общий размер файла
        ofs.write(reinterpret_cast<char*>(&bitmap_file_header), sizeof(BitmapFileHeader)); // Запись заголовка файла в файл        
        BitmapInfoHeader bitmap_info_header;
        bitmap_info_header.img_width = width;
        bitmap_info_header.img_height = height;
        bitmap_info_header.data_size = bmp_stride * height;                                // Размер данных изображения
        ofs.write(reinterpret_cast<char*>(&bitmap_info_header), sizeof(BitmapInfoHeader)); // Запись заголовка информации в файл        
        vector<char> buffer(bmp_stride);                          // Буфер для хранения строки пикселей
        // Запись каждого пикселя в файл
        for (int y = height - 1; y >= 0; y--) {                   // Проходим строки изображения снизу вверх
            const Color* line = image.GetLine(y);                 // Получаем строку пикселей
            for (int x = 0; x < width; ++x) {
                // Заполняем буфер значениями пикселей RGB
                buffer[x * 3 + 0] = static_cast<char>(line[x].b); // Синий
                buffer[x * 3 + 1] = static_cast<char>(line[x].g); // Зеленый
                buffer[x * 3 + 2] = static_cast<char>(line[x].r); // Красный
            }
            ofs.write(buffer.data(), bmp_stride);                 // Записываем строку в файл
        }
        ofs.close();                                              // Закрываем файл
        return true;
    }    
    // Загрузка изображения из BMP файла
    Image LoadBMP(const Path &file) {
        // Открытие файла
        ifstream ifs(file, ios::binary);
        if (!ifs) {
            return {};
        }
        BitmapFileHeader file_header{};
        if (!ifs.read(reinterpret_cast<char *>(&file_header), sizeof(BitmapFileHeader)) || 
            file_header.bm_type != 0x4D42) {                      // Проверка типа файла
            return {};
        }
        BitmapInfoHeader info_header{};
        if (!ifs.read(reinterpret_cast<char *>(&info_header), sizeof(BitmapInfoHeader)) || 
            info_header.bits_per_pixel != 24) {                   // Проверка бит на пиксель
            return {}; 
        }
        // Пропуск отступа к началу данных изображения
        ifs.seekg(file_header.padding);
        if (ifs.fail() || ifs.eof()) {
            return {}; 
        }
        // Создание результата с заданными размерами
        const std::int32_t width = info_header.img_width;
        const std::int32_t height = info_header.img_height;
        // Проверка, что размеры изображения корректные
        if (width <= 0 || height <= 0) {
            return {};
        }        
        Image result(width, height, Color::Black());                          // Черный фон по умолчанию
        const std::int32_t row_size = GetBMPStride(width);                    // Размер строки
        std::vector<std::uint8_t> buffer(row_size);                           // Буфер для хранения строки пикселей в памяти        
        // Чтение каждого пикселя из файла
        for (std::int32_t y = height - 1; y >= 0; --y) {                      // Проходим строки изображения снизу вверх
            ifs.read(reinterpret_cast<char *>(buffer.data()), buffer.size()); // Чтение строки пикселей
            if (ifs.gcount() < buffer.size()) {
                return {};
            }
            Color *line = result.GetLine(y);                                  // Получаем строку цвета в результате
            for (std::int32_t x = 0; x < width; ++x) {                        // Заполняем цвета из буфера
                line[x].b = static_cast<byte>(buffer[x * 3 + 0]);             // Синий
                line[x].g = static_cast<byte>(buffer[x * 3 + 1]);             // Зеленый
                line[x].r = static_cast<byte>(buffer[x * 3 + 2]);             // Красный
            }
        }
        return result;
    }
}
