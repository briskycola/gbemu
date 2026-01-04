#include "file.hpp"
#include "cartridge_metadata.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

void readFile(const std::string &filename)
{
    std::ifstream romFile;
    std::vector<uint8_t> romData;
    std::ifstream::pos_type readPosition;
    std::vector<char> fileContents;

    // Open the ROM file in binary mode
    // and move the read position
    // straight to the end of the file.
    //
    // This will easily give us the size of the file.
    romFile.open(filename.c_str(), std::ios::binary | std::ios::ate);
    if (!romFile)
    {
        std::cerr << "Could not open ROM file\n";
    }

    // Get the current read position, which
    // will give us the size of the file.
    // 
    // Then setup a dynamic array to store
    // the ROM file as a sequence of characters
    readPosition = romFile.tellg();
    fileContents = std::vector<char>(static_cast<size_t>(readPosition));

    // Change read position back to beginning
    // and read the entire ROM file from start
    // to finish
    romFile.seekg(0, std::ios::beg);
    romFile.read(&fileContents[0], static_cast<std::streamsize>(readPosition));
    romFile.close();

    // Store the file contents to a
    // dynamic array of bytes to better
    // represent the ROM data
    romData = std::vector<uint8_t>(fileContents.begin(), fileContents.end());
    getInfo(romData);
}
