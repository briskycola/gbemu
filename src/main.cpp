#include <iostream>
#include "file.hpp"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rom-file>\n";
        return 1;
    }
    readFile(argv[1]);
    return 0;
}
