#include <iostream> //for std::cout
#include <string.h> //for std::string
// #include <bits/stdc++.h>
#include <map>
#include <cstdlib>
#include "chunks.h"

using namespace std;

int main(int argc, char *argv[])
{
    for(int i =0;i<100; i++){
        pair<string, size_t> p = get_chunk(1);
        cout<<p.first<<" "<<p.second<<endl;
    }

    return 0;
}
