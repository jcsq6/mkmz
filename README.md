<br />
<div align="center">
  <a href="https://github.com/jcsq6/mkmz">
    <img src="images/logo.png" alt="Logo" width="800" height="400">
  </a>

  <p align="center">
    Png maze image generator.
  </p>
</div>

## About The Program

The goal of this program was to be able to generate mazes of any size as large as your computer can. This includes ridiculously large mazes. If you have a lot of memory, by all means, generate a 1,000,000*1,000,000 maze! The mazes are also very customizable, meaning that they don't have to be square. The same applies to the cell width and height-- the cells don't have to be square cells.

### Example Output  
<br />
<div align="center">
  <a href="https://github.com/jcsq6/mkmz">
    <img src="images/top.png" alt="example" width="453" height="153">
  </a>
</div>


### Built With

mkmz is built with [CMake](https://cmake.org/)

This program depends on the following libraries.

* [libpng](http://www.libpng.org/pub/png/libpng.html)
* [zlib](https://zlib.net/)

### Prerequisites
**Linux**
* libpng  
  `sudo apt-get install libpng-dev`
* zlib  
  `sudo apt-get install zlib1g-dev`  
  
**Windows (Vcpkg)**  
* libpng  
  `vcpkg install libpng:x64-windows-static` *For building a static x64 executable*  
* zlib  
  `vcpkg install zlib:x64-windows-static` *For building a static x64 executable*  

### Building
**Linux**
  1. `mkdir build`
  2. `cd build`
  3. `cmake ..`  
  4. `make` 
 
**Windows (Vcpkg)**
  1. `mkdir build`
  2. `cd build`
  3. `cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static ..`
  4. `msbuild mkmz.sln` *(From developer command prompt)*  
      *Alternative: open solution and build from visual studio*

## Usage  
**mkmz [*options*]** 

***options:***  
* ```--help```  
*Display help information*  
* ```--log```  
*Create log of program output*  
* ```--replace```  
*Replace any file with the same name*  
* ```-o <FileName>```  
*Specify name of output file (**Optional**)*  
* ```-width <MazeWidth>```  
*Specify width of maze in cells.*  
* ```-height <MazeHeight>```  
*Specify height of maze in cells.*  
* ```-cwidth <CellWidth>```  
*Specify width of cells in pixels of resulting image.*  
* ```-cheight <CellHeight>```  
*Specify height of cells in pixels of resulting image.*  

## Examples  
* ```mkmz -width 10 -height 15 -cwidth 10 -cheight 10```  
  *Generates maze with width 10, height 15, cell width 10, and cell height 10. Resulting image name is 10x15_maze.png*  
* ```mkmz -width 10 -cwidth 5```  
  *Generates maze with width 10, height 10, cell width 5, and cell height 5. Resulting image name is 10x10_maze.png*  
* ```mkmz 10 5 2 2```  
  *Generates maze with width 10, height 5, cell width 2, and cell height 2. Resulting image name is 10x5_maze.png*  
* ```mkmz 10 10 -o mymaze```  
  *Generates maze with width 10, height 10, cell width 10, and cell height 10. Resulting image name is mymaze.png*  

# Notes
* ***The cell width and cell height parameters reflect the distance between walls, not the area between them. So with a cell width of 2 and a cell height of 2, each passagway (or white part of the cell) will be 1 pixel.***  
* ***You can generate as big a maze as your computer will allow.***  
* ***The width, height, cell width, and cell height parameters can be ommited and their values can be set, in order***  
* ***If one parameter is given, and the related parameter is not, the related parameter's value will be the value of the given parameter***  

## Contact

JC Squires - jcsq6inquires@gmail.com
