#include <vector>
#include <fstream>
#include <string>
#include <memory>

struct FileChunk {
    uint16_t id;
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
        FileChunk f;
        f.id = id_ctr;
        infile.read(f.data, 1024);
        f.size = infile.gcount();

        v.push_back(std::shared_ptr<FileChunk>(&f));
    }

    return v;
}