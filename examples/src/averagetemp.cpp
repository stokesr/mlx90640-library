#include <stdint.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <chrono>
#include <thread>
#include "headers/MLX90640_API.h"
#include <ctime>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_NONE    "\x1b[30m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define FMT_STRING "%+06.1f "
//#define FMT_STRING "%+06.2f "
//#define FMT_STRING "\u2588\u2588"

#define MLX_I2C_ADDR 0x33

void write_header(std::ofstream &log_file, int readings, float averageTemp)
{
   time_t now;
   time(&now);
   char buf[sizeof "2011-10-08T07:07:09Z"];
   strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));

   log_file << buf << ',';
   log_file << readings << ',';
   log_file << averageTemp << std::endl;
}

void write_readings(std::ofstream &log_file, float mlx90640To[])
{
   for (int x = 0; x < 32; x++)
   {
      for (int y = 0; y < 24; y++)
      {
         float val = mlx90640To[32 * (23 - y) + x];

         log_file << val << ",";
      }
      log_file << std::endl;
   }
}

int main(){
    int state = 0;
    //printf("Starting...\n");
    static uint16_t eeMLX90640[832];
    float emissivity = 1;
    uint16_t frame[834];
    static float image[768];
    float eTa;
    static uint16_t data[768*sizeof(float)];
    std::ofstream logger_file;

    setbuf(stdout, NULL);

    std::fstream fs;

    MLX90640_SetDeviceMode(MLX_I2C_ADDR, 0);
    MLX90640_SetSubPageRepeat(MLX_I2C_ADDR, 0);
    MLX90640_SetRefreshRate(MLX_I2C_ADDR, 0b010);
    MLX90640_SetChessMode(MLX_I2C_ADDR);
    //MLX90640_SetSubPage(MLX_I2C_ADDR, 0);
    //printf("Configured...\n");

    paramsMLX90640 mlx90640;
    MLX90640_DumpEE(MLX_I2C_ADDR, eeMLX90640);
    MLX90640_ExtractParameters(eeMLX90640, &mlx90640);

    int refresh = MLX90640_GetRefreshRate(MLX_I2C_ADDR);
    //printf("EE Dumped...\n");

    int frames = 30;
    int subpage;
    static float mlx90640To[768];
    while (1){
        state = !state;
        //printf("State: %d \n", state);
        MLX90640_GetFrameData(MLX_I2C_ADDR, frame);
        // MLX90640_InterpolateOutliers(frame, eeMLX90640);
        eTa = MLX90640_GetTa(frame, &mlx90640);
        subpage = MLX90640_GetSubPageNumber(frame);
        MLX90640_CalculateTo(frame, &mlx90640, emissivity, eTa, mlx90640To);

        MLX90640_BadPixelsCorrection((&mlx90640)->brokenPixels, mlx90640To, 1, &mlx90640);
        MLX90640_BadPixelsCorrection((&mlx90640)->outlierPixels, mlx90640To, 1, &mlx90640);

        float average = 0.0;
        int in_range_readings = 0;
        int flame_readings = 0;
        float in_range_total = 0.0;

        for(int x = 0; x < 32; x++){
            for(int y = 0; y < 24; y++){
                float val = mlx90640To[32 * (23-y) + x];

                if ( (val > 70.0) && (val < 100.0) ) {
                   in_range_total+= val;
                   in_range_readings++;
                }
                if (val > 100.0) {
                   flame_readings++;
                }
            }
            //std::cout << std::endl;
        }
        if (in_range_total > 0) {
           average = in_range_total / in_range_readings;
        }
        printf("%f,%d,%d\n", average, in_range_readings, flame_readings);

        logger_file.open("temperature_readings_log.csv", std::ios::app);
        write_header(logger_file, flame_readings, average);
        write_readings(logger_file, mlx90640To);
        logger_file.close();

        std::this_thread::sleep_for(std::chrono::milliseconds(4000));
        //printf("\x1b[33A");
        //printf("\x1b[1A");
    }
    return 0;
}
