#include <iostream> //for std::cout
#include <string.h> //for std::string
// #include <bits/stdc++.h>
#include <map>
#include <cstdlib>
using namespace std;

size_t get_hash(string s){
    hash<string> h;
    return h(s);
}

pair<string, size_t> get_chunk(int id){
    map<int, string> m;
    
    m[1] = "The young man wanted a role model. He looked long and hard in his youth, but that role model never materialized. His only choice was to embrace all the people in his life he didnt want to be like.";
    m[2] = "She considered the birds to be her friends. Shed put out food for them each morning and then shed watch as they came to the feeders to gorge themselves for the day. She wondered what they would do if something ever happened to her.";
    m[3] = "There was a time when he would have embraced the change that was coming. In his youth, he sought adventure and the unknown, but that had been years ago. He wished he could go back and learn to find the excitement that came with change but it was useless.";
    m[4] = "She tried not to judge him. His ratty clothes and unkempt hair made him look homeless. Was he really the next Einstein as she had been told? On the off chance it was true, she continued to try not to judge him.";
    m[5] = "It was going to rain. The weather forecast didnt say that, but the steel plate in his hip did. He had learned over the years to trust his hip over the weatherman. It was going to rain, so he better get outside and prepare.";
    m[6] = "The headphones were on. They had been utilized on purpose. She could hear her mom yelling in the background, but couldnt make out exactly what the yelling was about";
    m[7] = "That was exactly why she had put them on. She knew her mom would enter her room at any minute, and she could pretend that she hadnt heard any of the previous yelling.";
    m[8] = "MaryLou wore the tiara with pride. There was something that made doing anything she didnt really want to do a bit easier when she wore it. She really didnt care what those staring through the window were thinking as she vacuumed her apartment.";
    m[9] = "He couldnt move. His head throbbed and spun. He couldnt decide if it was the flu or the drinking last night. It was probably a combination of both.";
    m[10] = "I recently discovered I could make fudge with just chocolate chips, sweetened condensed milk, vanilla extract, and a thick pot on slow heat.";

    string chunk = m[id];
    size_t hash = get_hash(chunk);
    int pCorrupt = rand()%3;
    if(pCorrupt==0) chunk = to_string(rand()) + ": This chunk was corrupted, please request a new one";
    pair<string, size_t> toret; toret.first = chunk; toret.second = hash;
    return toret;

}

