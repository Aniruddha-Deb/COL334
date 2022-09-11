#pragma once

#include <queue>
#include <fstream>
#include <string>
#include <memory>
#include <iostream>

struct FileChunk {
    uint32_t id;
    uint16_t size;
    char data[1024];
};

std::queue<std::shared_ptr<FileChunk>> read_and_chunk_file(std::string filename) {

    std::ifstream infile(filename);

    uint32_t id_ctr = 0;
    std::queue<std::shared_ptr<FileChunk>> v;

    while (!infile.eof()) {
        std::shared_ptr<FileChunk> fptr(new FileChunk);
        fptr->id = id_ctr;
        infile.read(fptr->data, 1024);
        fptr->size = infile.gcount();
        id_ctr++;

        v.push(fptr);
        std::cout << "Read chunk of " << fptr->size << " bytes from file" << std::endl;
    }

    return v;
}