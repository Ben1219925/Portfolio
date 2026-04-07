#include <fstream>
#include <chrono>
//#include <filesystem>
#include <cstdio>
#include <string>
#include <mpi.h>
#include <cmath>
using std::string;
using std::ios;
using std::ifstream;
using std::ofstream;

double duration(std::chrono::high_resolution_clock::time_point start, std::chrono::high_resolution_clock::time_point end )
{
  return std::chrono::duration<double, std::milli>(end - start).count();
}

int main(int argc, char ** argv)
{
  {
    MPI_Init(NULL,NULL);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    
    string filename = "HK-7_left_H6D-400c-MS-" + std::to_string(rank) +".bmp";
    
    // Open File
    ifstream fin(filename, ios::in | ios::binary);

    if(!fin.is_open()) {
      printf("File not opened\n");
      return -1;
    }
    // The first 14 bytes are the header, containing four values. Get those four values.
    char header[2];
    uint32_t filesize;
    uint32_t dummy;
    uint32_t offset;
    fin.read(header, 2);
    fin.read((char*)&filesize, 4);
    fin.read((char*)&dummy, 4);
    fin.read((char*)&offset, 4);
    printf("header: %c%c\n", header[0], header[1]);
    printf("filesize: %u\n", filesize);
    printf("dummy %u\n", dummy);
    printf("offset: %u\n", offset);
    int32_t sizeOfHeader;
    int32_t width;
    int32_t height;
    fin.read((char*)&sizeOfHeader, 4);
    fin.read((char*)&width, 4);
    fin.read((char*)&height, 4);
    printf("The width: %d\n", width);
    printf("The height: %d\n", height);
    uint16_t numColorPanes;
    uint16_t numBitsPerPixel;
    fin.read((char*)&numColorPanes, 2);
    fin.read((char*)&numBitsPerPixel, 2);
    printf("The number of bits per pixel: %u\n", numBitsPerPixel);
    if (numBitsPerPixel == 24) {
      printf("This bitmap uses rgb, where the first byte is blue, second byte is green, third byte is red.\n");
    }
    //uint32_t rowSize = (numBitsPerPixel * width + 31) / 32 * 4;
    //printf("Each row in the image requires %u bytes\n", rowSize);

    // Jump to offset where the bitmap pixel data starts
    fin.seekg(offset, ios::beg);

    // Read the data part of the file
    unsigned char* h_buffer = new unsigned char[filesize-offset];
    fin.read((char*)h_buffer, filesize-offset);
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;
    printf("The first pixel is located in the bottom left. Its blue/green/red values are (%u, %u, %u)\n", h_buffer[0],
    h_buffer[1], h_buffer[2]);
    printf("The second pixel is to the right. Its blue/green/red values are (%u, %u, %u)\n", h_buffer[3], h_buffer[4],
    h_buffer[5]);

    unsigned char* blue = new unsigned char[width*height];
    unsigned char* green = new unsigned char[width*height];
    unsigned char* red = new unsigned char[width*height];

    //make a copy to prevent overwriting original data when doing math on the arrays
    unsigned char* blue_copy = new unsigned char[width * height];
    unsigned char* green_copy = new unsigned char[width * height];
    unsigned char* red_copy = new unsigned char[width * height];
    int index = 0;
    for(uint32_t i = 0; i < filesize-offset; i+=3){
      blue[index] = h_buffer[i];
      index++;
    } 
    index = 0;
    for(uint32_t i = 1; i < filesize-offset; i+=3){
      green[index] = h_buffer[i];
      index++;
    }
    index = 0;
    for(uint32_t i = 2; i < filesize-offset; i+=3){
      red[index] = h_buffer[i];
      index++;
    }
    std::copy(blue, blue + width * height, blue_copy);
    std::copy(green, green + width * height, green_copy);
    std::copy(red, red + width * height, red_copy);

    delete[] h_buffer;

    unsigned char blue1[width*2];
    unsigned char green1[width*2];
    unsigned char red1[width*2];
    unsigned char blue2[width*2];
    unsigned char green2[width*2];
    unsigned char red2[width*2];

    
    MPI_Request requests[12];
    int top = width * (height - 2);
    //send necessary rows to neighbors
    printf("Starting: %d\n",rank);
    if(rank == 0) {
      MPI_Isend(blue_copy, width*2, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &requests[0]);
      MPI_Isend(green_copy, width*2, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &requests[1]);
      MPI_Isend(red_copy, width*2, MPI_CHAR, 1, 2, MPI_COMM_WORLD, &requests[2]);
      MPI_Irecv(blue1, width*2, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &requests[3]);
      MPI_Irecv(green1, width*2, MPI_CHAR, 1, 1, MPI_COMM_WORLD, &requests[4]);
      MPI_Irecv(red1, width*2, MPI_CHAR, 1, 2, MPI_COMM_WORLD, &requests[5]);    
      MPI_Waitall(6, requests, MPI_STATUSES_IGNORE);
    } 

    else if (rank == 3) {
      MPI_Isend(&blue_copy[top], width*2, MPI_CHAR, 2, 0, MPI_COMM_WORLD, &requests[0]);
      MPI_Isend(&green_copy[top], width*2, MPI_CHAR, 2, 1, MPI_COMM_WORLD, &requests[1]);
      MPI_Isend(&red_copy[top], width*2, MPI_CHAR, 2, 2, MPI_COMM_WORLD, &requests[2]);
      MPI_Irecv(blue1, width*2, MPI_CHAR, 2, 0, MPI_COMM_WORLD, &requests[3]);
      MPI_Irecv(green1, width*2, MPI_CHAR, 2, 1, MPI_COMM_WORLD, &requests[4]);
      MPI_Irecv(red1, width*2, MPI_CHAR, 2, 2, MPI_COMM_WORLD, &requests[5]);   
      MPI_Waitall(6, requests, MPI_STATUSES_IGNORE);
    } 

    else {
      MPI_Isend(blue_copy, width*2, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD, &requests[0]);
      MPI_Isend(green_copy, width*2, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &requests[1]);
      MPI_Isend(red_copy, width*2, MPI_CHAR, rank + 1, 2, MPI_COMM_WORLD, &requests[2]);
      MPI_Irecv(blue1, width*2, MPI_CHAR, rank + 1, 0, MPI_COMM_WORLD, &requests[3]);
      MPI_Irecv(green1, width*2, MPI_CHAR, rank + 1, 1, MPI_COMM_WORLD, &requests[4]);
      MPI_Irecv(red1, width*2, MPI_CHAR, rank + 1, 2, MPI_COMM_WORLD, &requests[5]);
 
      MPI_Isend(&blue_copy[top], width*2, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, &requests[6]);
      MPI_Isend(&green_copy[top], width*2, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &requests[7]);
      MPI_Isend(&red_copy[top], width*2, MPI_CHAR, rank - 1, 2, MPI_COMM_WORLD, &requests[8]);
      MPI_Irecv(blue2, width*2, MPI_CHAR, rank - 1, 0, MPI_COMM_WORLD, &requests[9]);
      MPI_Irecv(green2, width*2, MPI_CHAR, rank - 1, 1, MPI_COMM_WORLD, &requests[10]);
      MPI_Irecv(red2, width*2, MPI_CHAR, rank - 1, 2, MPI_COMM_WORLD, &requests[11]);
      MPI_Waitall(12, requests, MPI_STATUSES_IGNORE);
    }       
    printf("Done: blue1:%d blue2:%d blue[0]:%d blue[top]:%d rank:%d\n",blue1[0],blue2[0],blue[0],blue[top],rank);

    start = std::chrono::high_resolution_clock::now();
    float red_sum;
    float blue_sum;
    float green_sum;
    float weights[25] = {
      0.002969, 0.013306, 0.021938, 0.013306, 0.002969,
      0.013306, 0.059633, 0.098317, 0.059633, 0.013306,
      0.021938, 0.098317, 0.162098, 0.098317, 0.021938,
      0.013306, 0.059633, 0.098317, 0.059633, 0.013306,
      0.002969, 0.013306, 0.021938, 0.013306, 0.002969
    };

    if(rank == 0){
      for(int row = 0; row < height -2; row++){
        for(int col = 2; col < width - 2; col++){
          red_sum = 0;
          blue_sum = 0;
          green_sum = 0;
          index = 0;
          if(row > 1){
            for(int x = row-2; x <= row+2; x++){
              for(int y = col-2; y <= col+2; y++){
                red_sum += red_copy[x*width+y] * weights[index];
                blue_sum += blue_copy[x*width+y] * weights[index];
                green_sum += green_copy[x*width+y] * weights[index];
                index++;
              }
            }
          }

          else if (row == 1){
            for(int x = -1; x <= 3; x++) {
              for(int y = col - 2; y <= col + 2; y++) {
                if(x == -1) {
                  red_sum += red1[width + y] * weights[index];
                  blue_sum += blue1[width + y] * weights[index];
                  green_sum += green1[width + y] * weights[index];
                } 
                else{
                  red_sum += red_copy[x*width+y] * weights[index];
                  blue_sum += blue_copy[x*width+y] * weights[index];
                  green_sum += green_copy[x*width+y] * weights[index];
                }
                index++;
              }
            }
          } 

          else if(row == 0){
            for(int x = -2; x <= 2; x++){
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 0){
                  red_sum += red1[(2 + x) * width + y] * weights[index];
                  blue_sum += blue1[(2 + x) * width + y] * weights[index];
                  green_sum += green1[(2 + x) * width + y] * weights[index];
                } 
                else{
                  red_sum += red_copy[(row + x) * width + y] * weights[index];
                  blue_sum += blue_copy[(row + x) * width + y] * weights[index];
                  green_sum += green_copy[(row + x) * width + y] * weights[index];
                }
                index++;
              }
            }
          }
          red[row*width+col] = red_sum;
          blue[row*width+col] = blue_sum;
          green[row*width+col] = green_sum;
        }
      }
    }
    
    else if(rank == 3){
      for(int row = 2; row < height; row++){
        for(int col = 2; col < width - 2; col++){
          red_sum = 0;
          blue_sum = 0;
          green_sum = 0;
          index = 0;
          if(row < height-2){
            for(int x = row-2; x <= row+2; x++){
              for(int y = col-2; y <= col+2; y++){
                red_sum += red_copy[x*width+y] * weights[index];
                blue_sum += blue_copy[x*width+y] * weights[index];
                green_sum += green_copy[x*width+y] * weights[index];
                index++;
              }
            }
          }

          else if(row == height-2){
            for(int x = -2; x <= 2; x++) {
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 2) {
                  red_sum += red_copy[(row + x)*width + y] * weights[index];
                  blue_sum += blue_copy[(row + x)*width + y] * weights[index];
                  green_sum += green_copy[(row + x)*width + y] * weights[index];
                } 
                else{
                  red_sum += red1[y] * weights[index];
                  blue_sum += blue1[y] * weights[index];
                  green_sum += green1[y] * weights[index];
                }
                index++;
              }
            }
          } 

          else if(row == height-1){
            for(int x = -2; x <= 2; x++){
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 1){
                  red_sum += red_copy[(row + x) * width + y] * weights[index];
                  blue_sum += blue_copy[(row + x) * width + y] * weights[index];
                  green_sum += green_copy[(row + x) * width + y] * weights[index];
                } 
                else{
                  red_sum += red1[(x-1) * width + y] * weights[index];
                  blue_sum += blue1[(x-1) * width + y] * weights[index];
                  green_sum += green1[(x-1) * width + y] * weights[index];
                }
                index++;
              }
            }
          }
          red[row*width+col] = red_sum;
          blue[row*width+col] = blue_sum;
          green[row*width+col] = green_sum;
        }
      }
    }

    else{
      for(int row = 0; row < height; row++){
        for(int col = 2; col < width - 2; col++){
          red_sum = 0;
          blue_sum = 0;
          green_sum = 0;
          index = 0;
         if(row > 1 && row < height-2){
            for(int x = row-2; x <= row+2; x++){
              for(int y = col-2; y <= col+2; y++){
                red_sum += red_copy[x*width+y] * weights[index];
                blue_sum += blue_copy[x*width+y] * weights[index];
                green_sum += green_copy[x*width+y] * weights[index];
                index++;
              }
            }
          }
          
          else if(row == height-2){
            for(int x = -2; x <= 2; x++) {
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 2) {
                  red_sum += red_copy[(row + x)*width + y] * weights[index];
                  blue_sum += blue_copy[(row + x)*width + y] * weights[index];
                  green_sum += green_copy[(row + x)*width + y] * weights[index];
                } 
                else{
                  red_sum += red2[y] * weights[index];
                  blue_sum += blue2[y] * weights[index];
                  green_sum += green2[y] * weights[index];
                }
                index++;
              }
            }
          } 

          else if(row == height-1){
            for(int x = -2; x <= 2; x++){
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 1){
                  red_sum += red_copy[(row + x) * width + y] * weights[index];
                  blue_sum += blue_copy[(row + x) * width + y] * weights[index];
                  green_sum += green_copy[(row + x) * width + y] * weights[index];
                } 
                else{
                  red_sum += red2[(x-1) * width + y] * weights[index];
                  blue_sum += blue2[(x-1) * width + y] * weights[index];
                  green_sum += green2[(x-1) * width + y] * weights[index];
                }
                index++;
              }
            }
          }
          
          else if (row == 1){
            for(int x = -1; x <= 3; x++) {
              for(int y = col - 2; y <= col + 2; y++) {
                if(x == -1) {
                  red_sum += red1[width + y] * weights[index];
                  blue_sum += blue1[width + y] * weights[index];
                  green_sum += green1[width + y] * weights[index];
                } 
                else{
                  red_sum += red_copy[x*width+y] * weights[index];
                  blue_sum += blue_copy[x*width+y] * weights[index];
                  green_sum += green_copy[x*width+y] * weights[index];
                }
                index++;
              }
            }
          } 

          else if(row == 0){
            for(int x = -2; x <= 2; x++){
              for(int y = col - 2; y <= col + 2; y++) {
                if(x < 0){
                  red_sum += red1[(2 + x) * width + y] * weights[index];
                  blue_sum += blue1[(2 + x) * width + y] * weights[index];
                  green_sum += green1[(2 + x) * width + y] * weights[index];
                } 
                else{
                  red_sum += red_copy[(row + x) * width + y] * weights[index];
                  blue_sum += blue_copy[(row + x) * width + y] * weights[index];
                  green_sum += green_copy[(row + x) * width + y] * weights[index];
                }
                index++;
              }
            }
          }

          red[row*width+col] = red_sum;
          blue[row*width+col] = blue_sum;
          green[row*width+col] = green_sum;
        }
      }

    }


    end = std::chrono::high_resolution_clock::now();
    printf("Time - %g ms Rank - %d\n", duration(start,end),rank);


    if(rank == 2){
      printf("The red, green, blue at (8353, 9111) (origin bottom left) is (%d, %d, %d)\n", red[4003*width+9111], green[4003*width+9111], blue[4003*width+9111]);
      printf("The red, green, blue at (8351, 9113) (origin bottom left) is (%d, %d, %d)\n", red[4001*width+9113], green[4001*width+9113], blue[4001*width+9113]);
      printf("The red, green, blue at (6352, 15231) (origin bottom left) is (%d, %d, %d)\n", red[2002*width+15231], green[2002*width+15231], blue[2002*width+15231]);
    }
    if(rank == 1){
      printf("The red, green, blue at (10559, 10611) (origin bottom left) is (%d, %d, %d)\n", red[1859*width+10611], green[1859*width+10611], blue[1859*width+10611]); 
      printf("The red, green, blue at (10818, 20226) (origin bottom left) is (%d, %d, %d)\n", red[2118*width+20226], green[2118*width+20226], blue[2118*width+20226]);
    }

    //Print out to file output
    string outputFile = "output-" + std::to_string(rank) + ".bmp";
    ofstream fout;
    fout.open(outputFile, ios::binary);

    // Copy of the old headers into the new output
    fin.seekg(0, ios::beg);
    // Read the data part of the file
    char* headers = new char[offset];
    fin.read(headers, offset);
    fout.seekp(0, ios::beg);
    fout.write(headers, offset);
    delete[] headers;

    fout.seekp(offset, ios::beg);
    
    for(uint32_t i = 0; i < (filesize-offset)/3; i++){
      fout.put(blue[i]);
      fout.put(green[i]);
      fout.put(red[i]);
    }
    fout.close();
    delete[] red;
    delete[] blue;
    delete[] green;
    delete[] red_copy;
    delete[] blue_copy;
    delete[] green_copy;
    MPI_Finalize();

  }
}
