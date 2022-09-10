#pragma once

#include <vector>
#include <fstream>
#include <string>
#include <memory>

struct FileChunk {
    uint32_t id;
    uint16_t size;
    char data[1024];
};

std::vector<std::shared_ptr<FileChunk>> read_and_chunk_file(std::string filename) {

    std::ifstream infile(filename);

    infile.seekg(0, std::ios::end);
    size_t length = infile.tellg();
    infile.seekg(0, std::ios::beg);

    uint16_t id_ctr = 1;
    std::vector<std::shared_ptr<FileChunk>> v;

    while (infile.eofbit != 1) {
        std::shared_ptr<FileChunk> fptr(new FileChunk);
        fptr->id = id_ctr;
        infile.read(fptr->data, 1024);
        fptr->size = infile.gcount();

        v.push_back(fptr);
    }

    return v;
}