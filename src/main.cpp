//
//  main.cpp
//  mix
//
//  Created by The Phantom of the Soap Opera on 22.10.2015.
//  Copyright © 2015 The Phantom of the Soap Opera. All rights reserved.
//

#include <assert.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <poll.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

using namespace std;

int argNum;
const char** argVal;

unsigned long allWordCount = 0;
int wordCount = 0;
char** words = nullptr;
int** wordsByLength = nullptr;
int* wordsByLengthCount = nullptr;
int wordsByLengthNumber = 0;
int minLength = -1;
int maxLength = -1;
int numPositions = -1;
int minNumPositions = -1;
int maxNumPositions = -1;
long cpersec = -1;
int* wordUsage = nullptr;

int wordsIgnored = 0;
const int ignoreShorterDefault = 1;
int ignoreShorterThan = ignoreShorterDefault;
const int ignoreLongerDefault = 64;
int ignoreLongerThan = -1;

std::vector<const char*> charset;

int defaultMinChar = 8;
int minChar = -1;
int maxChar = -1;
std::vector<const char*> inFile;
std::ifstream ifinput;
const char* outFile = nullptr;
std::ostream *output;
std::ofstream ofoutput;
int replaceCount = 0;
char** replaceList = nullptr;
bool replaceFirst = false;
bool downFirst = false;
bool replaceRandom = false;
bool toLower = false;
bool toUpper = false;
bool norepeat = false;
bool reduceRepeats = false;
bool noWork = false;
bool benchmark = false;
long benchmarkDuration = 10;
const char* benchmarkOutput = "/dev/null";
const char* benchmarkCharset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

bool quit = false;
bool normalFinish = false;
bool show = false;

long linesWritten = 0;
long resumeAt = 0;
int resumeAtPattern = 0;
const char* resumeData = nullptr;
long quitAt = -1;

long startTime;
unsigned long long bytesWritten = 0;

std::map<int, std::vector<std::vector<int>>> producedPatterns;

const int defChildInstances = 1;
int childInstances = 0;
bool separatedChildOutput = false;
bool useBlocking = false;
bool useChildWriteOption = false;
const char* providedPath = nullptr;

const char* defPyritPath = "pyrit";
const char* pyrit = "%s -r %s -i - attack_passthrough";
const char* pyritwOutput = "%s -r %s -i - attack_passthrough >> %s 2>&1";
const char* pyritwSeparatedOutput = "%s -r %s -i - attack_passthrough >> %s-%d 2>&1";
const char* pyritwOutputToFile = "%s -r %s -i - -o %s attack_passthrough >/dev/null 2>&1";
const char* pyritwSeparatedOutputToFile = "%s -r %s -i - -o %s-%d attack_passthrough >/dev/null 2>&1";

const char* defHashcatPath = "oclHashcat";
const char* hashcat = "%s -m 2500 --workload-profile=3 %s";
const char* hashcatwOutput = "%s -m 2500 --workload-profile=3 --potfile-disable --logfile-disable %s >> %s 2>&1";
const char* hashcatwOutputToFile = "%s -m 2500 --workload-profile=3 --quiet --potfile-disable --logfile-disable --outfile=%s %s >/dev/null 2>&1";

FILE** poutput = nullptr;
int* dpoutput = nullptr;
struct pollfd* dpolloutput = nullptr;
int dpollcount = 0;
const char* capFile = nullptr;
int childToUse = 0;
const int USE_PYRIT = 1;
const int USE_OCLHASHCAT = 2;

const char* argDir = nullptr;
const char* argFile = nullptr;
const char* oFile = nullptr;
long oFileCycle = 1000000;

void premix();
void just_print();
void push(char* buffer, int len);
void resetCounters();
void producePatterns(int minLen, int maxLen);
std::vector<std::vector<int>> getPatterns(int forLen);
std::vector<std::vector<int>> getPatterns(int minLen, int maxLen);
std::string getResumeData();
void pattern();
void cleanup();
void write_data();

struct WordDesc {
    const char** subwords;
    int subWordCount;
    int* lastUsed;
};

WordDesc* dictionary;

struct WordDef {
    const char* word;
    int index;
};

void ctrlc_handler(int s) {
    quit = true;
}

void pipe_handler(int s) {
    if (!quit && oFile && capFile) {
        std::cerr << "Pipe broken. Probably pyrit finished work." << std::endl;
        remove(oFile);
        oFile = nullptr;
    }
    quit = true;
}

void info_handler(int s) {
    show = true;
}

std::string getResumeData() {
    std::string ret;
    char fmt[100];
    sprintf(fmt,"%d:%d:%d@", wordCount, resumeAtPattern, minChar);
    ret += fmt;
    for(int i = 0 ; i < numPositions ; i++) {
        int usage = wordUsage[i];
        sprintf(fmt,"%d",usage);
        ret += fmt;
        if (usage >= 0) {
            WordDesc* wd = &dictionary[usage];
            for(int j = 0 ; j < numPositions ; j++) {
                int lastUsed = wd->lastUsed[j];
                bool last = true;
                for(int k = j + 1 ; k < numPositions ; k++) {
                    if (wd->lastUsed[k] > 0) {
                        last = false;
                        break;
                    }
                }
                if (lastUsed == 0 && last) {
                    break;
                }
                sprintf(fmt, j == 0 ? (last ? ":%d" : ":%d#") : (last ? "%d" : "%d#"), lastUsed);
                ret += fmt;
                if (last) {
                    break;
                }
            }
        }
        if (i < numPositions - 1) {
            ret += "@";
        }
    }
    return ret;
}

void write_data() {
    long totalTimeSec = startTime > 0 ? time(nullptr) - startTime : 0;
    std::cerr << std::endl << "Total time used: " << totalTimeSec << " seconds" << std::endl;
    std::cerr << "Lines written: " << linesWritten;
    if (!normalFinish && !benchmark) {
        std::cerr << " (use -x to resume)";
    }
    std::cerr << std::endl;
    if (!normalFinish && !benchmark) {
        std::string ret = getResumeData();
        std::cerr << "Fast resume data (use -z<data> for resume): " << ret.c_str() << std::endl;
        std::cerr << "Complete line to resume is:" << std::endl << argVal[0] << " -z" << ret.c_str() << " ";
        bool ignoreNext = false;
        for(int i = 1 ; i < argNum ; i++) {
            const char* nxtArg = argVal[i];
            size_t len = strlen(nxtArg);
            bool isZ = strncmp(nxtArg, "-z", 2) == 0;
            if (!isZ && !ignoreNext) {
                std::cerr << nxtArg << " ";
            }
            ignoreNext = isZ && len == 2;
        }
        std::cerr << std::endl << std::endl;
    }
    if (totalTimeSec > 0) {
        linesWritten /= totalTimeSec;
        std::cerr << "Performance is " << linesWritten << " lines/sec";
        linesWritten /= 1000;
        if (linesWritten > 0) {
            std::cerr << " or " << linesWritten << " klines/sec";
            linesWritten /= 1000;
            if (linesWritten > 0) {
                std::cerr << " or " << linesWritten << " mlines/sec";
                linesWritten /= 1000;
                if (linesWritten > 0) {
                    std::cerr << " or " << linesWritten << " glines/sec";
                    linesWritten /= 1000;
                    if (linesWritten > 0) {
                        std::cerr << " or " << linesWritten << " tlines/sec";
                        linesWritten /= 1000;
                        if (linesWritten > 0) {
                            std::cerr << " or " << linesWritten << " plines/sec";
                        }
                    }
                }
            }
        }
    }
    std::cerr << std::endl;
    std::cerr << "Bytes written: " << bytesWritten;
    unsigned long long xbw = bytesWritten;
    bytesWritten /= 1024;
    if (bytesWritten > 0) {
        std::cerr << ", " << bytesWritten << "KB";
        bytesWritten /= 1024;
        if (bytesWritten > 0) {
            std::cerr << ", " << bytesWritten << "MB";
            bytesWritten /= 1024;
            if (bytesWritten > 0) {
                std::cerr << ", " << bytesWritten << "GB";
                bytesWritten /= 1024;
                if (bytesWritten > 0) {
                    std::cerr << ", " << bytesWritten << "TB";
                    bytesWritten /= 1024;
                    if (bytesWritten > 0) {
                        std::cerr << ", " << bytesWritten << "PB";
                        bytesWritten /= 1024;
                    }
                }
            }
        }
    }
    std::cerr << std::endl;
    if (totalTimeSec > 0) {
        xbw /= totalTimeSec;
        std::cerr << "Performance is " << xbw << " bytes/sec";
        xbw /= 1024;
        if (xbw > 0) {
            std::cerr << " or " << xbw << " KB/sec";
            xbw /= 1024;
            if (xbw > 0) {
                std::cerr << " or " << xbw << " MB/sec";
                xbw /= 1024;
                if (xbw > 0) {
                    std::cerr << " or " << xbw << " GB/sec";
                    xbw /= 1024;
                    if (xbw > 0) {
                        std::cerr << " or " << xbw << " TB/sec";
                        xbw /= 1024;
                        if (xbw > 0) {
                            std::cerr << " or " << xbw << " PB/sec";
                        }
                    }
                }
            }
        }
    }
    std::cerr << std::endl;
    std::cerr << std::endl;
}

void usage() {
    std::cerr << "This program will generate a mix from given word list" << std::endl;
    std::cerr << "    (version 0.6)" << std::endl;
    std::cerr << std::endl;
    std::cerr << " Usage:" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    mix [options] <word1> <word2> [word3] ...." << std::endl;
    std::cerr << std::endl;
    std::cerr << "            (ctrl-c on running application will produce benchmarks)" << std::endl;
    std::cerr << std::endl;
    std::cerr << std::endl;
    std::cerr << " Options: (please note that you may separate values from option or concatenate them)" << std::endl;
    std::cerr << "  -min<number>     - minimum number of characters (default 8)" << std::endl;
    std::cerr << "  -max<number>     - maximum number of characters (recommended to use this)" << std::endl;
    std::cerr << "  -s<charset>      - use provided charset" << std::endl;
    std::cerr << "  -f<path>         - read basic wordset from file instead of stdin" << std::endl;
    std::cerr << "  -w<path>         - write generated wordset to file instead of stdout, if used with -p or -h, then pyrit/oclHashcat output will be written to file" << std::endl;
    std::cerr << "  -W<path>         - same as -w, except it will use write option of pyrit/oclHashcat instead of stdout redirection to file; must be used with -p or -h" << std::endl;
    std::cerr << "  -r<char1><char2> - generate more words by replacing char1 with char2 (case-sensitive)" << std::endl;
    std::cerr << "  -R<char1><char2> - generate more words by replacing char1 with char2 (case-insensitive)" << std::endl;
    std::cerr << "  -l<number>       - minimum number of mixed words (recommended to use this)" << std::endl;
    std::cerr << "  -L<number>       - maximum number of mixed words (recommended to use this)" << std::endl;
    std::cerr << "  -i<number>       - ignore words shorter than certain number of characters (1 by default). If set and -min is not used," << std::endl;
    std::cerr << "                     it will also override -min default setting." << std::endl;
    std::cerr << "  -I<number>       - ignore words longer than certain number of characters (64 by default if -max" << std::endl;
    std::cerr << "                     is not used, if -max is used, then by default equal to -max; -I supercedes -max)" << std::endl;
    std::cerr << "  -n               - do not repeat words in phrase" << std::endl;
    std::cerr << "  -tl              - pre-process all words to lowercase" << std::endl;
    std::cerr << "  -tu              - pre-process all words to uppercase" << std::endl;
    std::cerr << "  -c               - generate more words by capitalizing first letter of the word" << std::endl;
    std::cerr << "  -d               - generate more words by decapitalizing first letter of the word" << std::endl;
    std::cerr << "  -u               - generate more words by permutatively capitalizing all letters of the word (supercedes -c)." << std::endl;
    std::cerr << "                     This will also lowercase all the words given. Use with caution." << std::endl;
    std::cerr << "  -e               - eliminate repeating words, this may take a very long time" << std::endl;
    std::cerr << "  -x<number>       - resume at specific line number, cannot be used with -z." << std::endl;
    std::cerr << "  -z<data>         - resume at specific program state, fast, but requires special data format, excludes -x." << std::endl;
    std::cerr << "  -q<number>       - quit at specific line number" << std::endl;
    std::cerr << "  -b               - run benchmark only for 10 seconds, this excludes all other options" << std::endl;
    std::cerr << "  -v[number]       - number specifies operations per second; this option will give additional" << std::endl;
    std::cerr << "  -p<path>         - path to .cap file, this will pipe pyrit command with parameters:" << std::endl;
    std::cerr << "                       pyrit -r <path> -i - attack_passthrough" << std::endl;
    std::cerr << "  -P<number>       - number of instances of pyrit to run, default 1; use only with -p<>." << std::endl;
    std::cerr << "                     Cannot be used with -h" << std::endl;
    std::cerr << "  -h<path>         - path to .hccap file, this will pipe oclHashcat command with parameters:" << std::endl;
    std::cerr << "                       oclHashcat -m 2500 --workload-profile=3 <path>" << std::endl;
    std::cerr << "                     This will also use blocking output (-B)" << std::endl;
    std::cerr << "  -S               - separate pyrit outputs when -w is used. This option requires -P and -w options." << std::endl;
    std::cerr << "                     Cannot be used with -h" << std::endl;
    std::cerr << "  -B               - Use blocking output to pyrit/oclHashcat. This option requires -p or -h option." << std::endl;
    std::cerr << "                     With -h, it is on by default." << std::endl;
    std::cerr << "  -g<path>         - complete path to pyrit/oclHashcat, if relative does not work" << std::endl;
    std::cerr << "  -a<path>         - read arguments from file, cannot be used with other arguments" << std::endl;
    std::cerr << "  -h<dir path>     - read all files from given directory and invoke this program with -a<path>" << std::endl;
    std::cerr << "                     for each file found, this option cannot be used with any other options" << std::endl;
    std::cerr << "  -o<path>         - write regular update of state to given file, this file can be used with -a option" << std::endl;
    std::cerr << "  -O<number>       - regular update write in each of <number> cycles; this option requires -o<path>" << std::endl;
    std::cerr << "                     if not specified, then 1000000 is used" << std::endl;
    std::cerr << std::endl;
    std::cerr << "                     Note: use SIGUSR1 to trigger printout of current word." << std::endl;
    std::cerr << std::endl;
    std::cerr << std::endl;
}

bool sorter(const char* a, const char*b) {
    return strcmp(a,b) < 0;
}

bool lengther(const WordDef& a, const WordDef& b) {
    return strlen(a.word) < strlen(b.word);
}

int main(int argc, const char * argv[]) {
    // remember args
    argNum = argc;
    argVal = argv;
    // setup ctrl-c handler
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = ctrlc_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    struct sigaction sigPipeHandler;
    sigPipeHandler.sa_handler = pipe_handler;
    sigemptyset(&sigPipeHandler.sa_mask);
    sigPipeHandler.sa_flags = 0;
    sigaction(SIGPIPE, &sigPipeHandler, NULL);
    struct sigaction sigInfoHandler;
    sigInfoHandler.sa_handler = info_handler;
    sigemptyset(&sigInfoHandler.sa_mask);
    sigInfoHandler.sa_flags = 0;
    sigaction(SIGUSR1, &sigInfoHandler, NULL);
    // remove buffering
    std::cerr.sync_with_stdio(false);
    // continue
    srand((unsigned int)time(NULL));
    if (argc <= 1) {
        usage();
        return 0;
    }
    assert(argv);
    vector<char*> replacev;
    bool anyOption = false;
    const char* lastCommand = nullptr;
    for(int i = 1 ; i < argc ; i++) {
        const char* arg = argv[i];
        int alen = (int)strlen(arg);
        if (alen <=0) {
            continue;
        }
        if ((arg[0] == '-' || lastCommand) && wordCount <= 0) {
            // parse argument
            const char* rarg = &arg[lastCommand ? 0 : 1];
            if ((!lastCommand && strncmp(rarg, "min", 3) == 0) || (lastCommand && strncmp(lastCommand, "min", 3) == 0)) {
                if (minChar >= 0) {
                    std::cerr << "-min cannot be used multiple times." << std::endl;
                    return -1;
                }
                rarg = &arg[lastCommand ? 0 : 4];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-min requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                minChar = atoi(rarg);
                if (minChar <= 0) {
                    std::cerr << "-min requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "max", 3) == 0) || (lastCommand && strncmp(lastCommand, "max", 3) == 0)) {
                if (maxChar >= 0) {
                    std::cerr << "-max cannot be used multiple times." << std::endl;
                    return -1;
                }
                rarg = &arg[lastCommand ? 0 : 4];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-max requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                maxChar = atoi(rarg);
                if (ignoreLongerThan <= 0) {
                    ignoreLongerThan = maxChar;
                }
                if (maxChar <= 0) {
                    std::cerr << "-max requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "s", 1) == 0) || (lastCommand && strncmp(lastCommand, "s", 1) == 0)) {
                const char* chars = &rarg[lastCommand ? 0 : 1];
                if (strlen(chars) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-s requires at least one character more: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                charset.push_back(chars);
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "f", 1) == 0) || (lastCommand && strncmp(lastCommand, "f", 1) == 0)) {
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-f requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                inFile.push_back(rarg);
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "w", 1) == 0) || (lastCommand && strncmp(lastCommand, "w", 1) == 0)) {
                if (outFile != nullptr) {
                    std::cerr << "-w/-W cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-w/-W requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                outFile = rarg;
                useChildWriteOption = false;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "W", 1) == 0) || (lastCommand && strncmp(lastCommand, "W", 1) == 0)) {
                if (outFile != nullptr) {
                    std::cerr << "-W/-w cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-W/-w requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                outFile = rarg;
                useChildWriteOption = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "n", 1) == 0) {
                if (norepeat) {
                    std::cerr << "-n cannot be used multiple times." << std::endl;;
                    return -1;
                }
                norepeat = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "tl", 2) == 0) {
                if (toLower) {
                    std::cerr << "-tl cannot be used multiple times." << std::endl;;
                    return -1;
                }
                if (toUpper) {
                    std::cerr << "-tl cannot be used with -tu." << std::endl;
                    return -1;
                }
                toLower = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "tu", 2) == 0) {
                if (toUpper) {
                    std::cerr << "-tu cannot be used multiple times." << std::endl;;
                    return -1;
                }
                if (toLower) {
                    std::cerr << "-tu cannot be used with -tl." << std::endl;
                    return -1;
                }
                toUpper = true;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "r", 1) == 0) || (lastCommand && strncmp(lastCommand, "r", 1) == 0)) {
                const char* rrarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rrarg) != 2) {
                    if (lastCommand) {
                        std::cerr << "-r requires exactly 2 characters afterwards: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                char* replac = new char[2];
                replac[0] = rrarg[0];
                replac[1] = rrarg[1];
                bool found = false;
                for(std::vector<char*>::iterator it = replacev.begin() ; it != replacev.end(); ++it) {
                    if (replac[0] == (*it)[0] || replac[1] == (*it)[0]) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    std::cerr << "-r given repeated or contraindicatory pattern: " << arg << std::endl;
                    return -1;
                }
                replacev.push_back(replac);
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "R", 1) == 0) || (lastCommand && strncmp(lastCommand, "R", 1) == 0)) {
                const char* rrarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rrarg) != 2) {
                    if (lastCommand) {
                        std::cerr << "-r requires exactly 2 characters afterwards: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                char* replac = new char[2];
                char lower = tolower(rrarg[0]);
                replac[0] = lower;
                replac[1] = rrarg[1];
                bool found = false;
                for(std::vector<char*>::iterator it = replacev.begin() ; it != replacev.end(); ++it) {
                    if (replac[0] == (*it)[0] || replac[1] == (*it)[0]) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    std::cerr << "-R given repeated or contraindicatory pattern: " << arg << std::endl;
                    return -1;
                }
                replacev.push_back(replac);
                char upper = toupper(rrarg[0]);
                if (lower != upper) {
                    replac = new char[2];
                    replac[0] = upper;
                    replac[1] = rrarg[1];
                    found = false;
                    for(std::vector<char*>::iterator it = replacev.begin() ; it != replacev.end(); ++it) {
                        if (replac[0] == (*it)[0] || replac[1] == (*it)[0]) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        std::cerr << "-R given repeated or contraindicatory pattern: " << arg << std::endl;
                        return -1;
                    }
                    replacev.push_back(replac);
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "l", 1) == 0) || (lastCommand && strncmp(lastCommand, "l", 1) == 0)) {
                if (minNumPositions > 0) {
                    std::cerr << "-l cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-l requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                minNumPositions = atoi(rarg);
                if (minNumPositions <= 0) {
                    std::cerr << "-l requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "L", 1) == 0) || (lastCommand && strncmp(lastCommand, "L", 1) == 0)) {
                if (maxNumPositions > 0) {
                    std::cerr << "-L cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-L requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                maxNumPositions = atoi(rarg);
                if (maxNumPositions <= 0) {
                    std::cerr << "-L requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "i", 1) == 0) || (lastCommand && strncmp(lastCommand, "i", 1) == 0)) {
                if (ignoreShorterThan > 1) {
                    std::cerr << "-i cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-i requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                ignoreShorterThan = atoi(rarg);
                if (minChar <= 0) {
                    minChar = ignoreShorterThan;
                }
                if (ignoreShorterThan <= 0) {
                    std::cerr << "-i requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "I", 1) == 0) || (lastCommand && strncmp(lastCommand, "I", 1) == 0)) {
                if (ignoreLongerThan > 0) {
                    std::cerr << "-I cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-I requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                ignoreLongerThan = atoi(rarg);
                if (ignoreLongerThan <= 0) {
                    std::cerr << "-I requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "c", 1) == 0) {
                if (replaceFirst) {
                    std::cerr << "-c cannot be used multiple times." << std::endl;;
                    return -1;
                }
                replaceFirst = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "d", 1) == 0) {
                if (downFirst) {
                    std::cerr << "-d cannot be used multiple times." << std::endl;;
                    return -1;
                }
                downFirst = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "u", 1) == 0) {
                if (replaceRandom) {
                    std::cerr << "-u cannot be used multiple times." << std::endl;;
                    return -1;
                }
                replaceRandom = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "e", 1) == 0) {
                if (reduceRepeats) {
                    std::cerr << "-e cannot be used multiple times." << std::endl;;
                    return -1;
                }
                reduceRepeats = true;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "x", 1) == 0) || (lastCommand && strncmp(lastCommand, "x", 1) == 0)) {
                if (resumeAt > 0) {
                    std::cerr << "-x cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-x requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                resumeAt = atol(rarg);
                if (resumeAt <= 0) {
                    std::cerr << "-x requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "z", 1) == 0) || (lastCommand && strncmp(lastCommand, "z", 1) == 0)) {
                if (resumeData != nullptr) {
                    std::cerr << "-z cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-z requires data: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                resumeData = rarg;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "q", 1) == 0) || (lastCommand && strncmp(lastCommand, "q", 1) == 0)) {
                if (quitAt > 0) {
                    std::cerr << "-q cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-q requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                quitAt = atol(rarg);
                if (quitAt <= 0) {
                    std::cerr << "-q requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "p", 1) == 0) || (lastCommand && strncmp(lastCommand, "p", 1) == 0)) {
                if (capFile != nullptr) {
                    std::cerr << "-p/-h cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-p requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                capFile = rarg;
                childToUse = USE_PYRIT;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "h", 1) == 0) || (lastCommand && strncmp(lastCommand, "h", 1) == 0)) {
                if (capFile != nullptr) {
                    std::cerr << "-h/-p cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-h requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                capFile = rarg;
                childToUse = USE_OCLHASHCAT;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "P", 1) == 0) || (lastCommand && strncmp(lastCommand, "P", 1) == 0)) {
                if (childInstances > 0) {
                    std::cerr << "-P cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-P requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                childInstances = atoi(rarg);
                if (childInstances <= 0) {
                    std::cerr << "-P requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "S", 1) == 0) {
                if (separatedChildOutput) {
                    std::cerr << "-S cannot be used multiple times." << std::endl;;
                    return -1;
                }
                separatedChildOutput = true;
                anyOption = true;
            } else
            if (!lastCommand && strncmp(rarg, "B", 1) == 0) {
                if (useBlocking) {
                    std::cerr << "-B cannot be used multiple times." << std::endl;;
                    return -1;
                }
                useBlocking = true;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "g", 1) == 0) || (lastCommand && strncmp(lastCommand, "g", 1) == 0)) {
                if (providedPath != nullptr) {
                    std::cerr << "-g cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-g requires pyrit path: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                providedPath = rarg;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "a", 1) == 0) || (lastCommand && strncmp(lastCommand, "a", 1) == 0)) {
                if (argFile != nullptr) {
                    std::cerr << "-a cannot be used multiple times." << std::endl;;
                    return -1;
                }
                if (anyOption) {
                    std::cerr << "-a cannot be used with any other option." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-a requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                argFile = rarg;
            } else
            if ((!lastCommand && strncmp(rarg, "h", 1) == 0) || (lastCommand && strncmp(lastCommand, "h", 1) == 0)) {
                if (argDir != nullptr) {
                    std::cerr << "-h cannot be used multiple times." << std::endl;;
                    return -1;
                }
                if (anyOption) {
                    std::cerr << "-h cannot be used with any other option." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-h requires directory name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                argDir = rarg;
            } else
            if ((!lastCommand && strncmp(rarg, "o", 1) == 0) || (lastCommand && strncmp(lastCommand, "o", 1) == 0)) {
                if (oFile != nullptr) {
                    std::cerr << "-o cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-o requires file name: " << arg << std::endl;;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                oFile = rarg;
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "O", 1) == 0) || (lastCommand && strncmp(lastCommand, "O", 1) == 0)) {
                if (oFileCycle > 0) {
                    std::cerr << "-O cannot be used multiple times." << std::endl;;
                    return -1;
                }
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-O requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                oFileCycle = atol(rarg);
                if (oFileCycle <= 0) {
                    std::cerr << "-O requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else
            if ((!lastCommand && strncmp(rarg, "b", 1)) == 0) {
                if (benchmark) {
                    std::cerr << "-b cannot be used multiple times." << std::endl;;
                    return -1;
                }
                benchmark = true;
            } else
            if ((!lastCommand && strncmp(rarg, "v", 1) == 0) || (lastCommand && strncmp(lastCommand, "O", 1) == 0)) {
                if (noWork) {
                    std::cerr << "-v cannot be used multiple times." << std::endl;;
                    return -1;
                }
                noWork = true;
                rarg = &rarg[lastCommand ? 0 : 1];
                if (strlen(rarg) <= 0) {
                    if (lastCommand) {
                        std::cerr << "-v requires number: " << arg << std::endl;
                        return -1;
                    } else {
                        lastCommand = &arg[1];
                        continue;
                    }
                }
                lastCommand = nullptr;
                cpersec = atol(rarg);
                if (cpersec <= 0) {
                    std::cerr << "-v requires number: " << arg << std::endl;
                    return -1;
                }
                anyOption = true;
            } else {
                std::cerr << "Invalid argument: " << arg << std::endl;
                return -1;
            }
        } else {
            if (benchmark) {
                std::cerr << "Benchmarking excludes using wordlist." << std::endl;
                return -1;
            }
            if (ignoreLongerThan <= 0) {
                ignoreLongerThan = ignoreLongerDefault;
            }
            if (ignoreShorterThan > ignoreLongerThan) {
                std::cerr << "-i and -I given contradictory values." << std::endl;
                return -1;
            }
            if (alen >= ignoreShorterThan && alen <= ignoreLongerThan) {
                if (!words) {
                    words = new char*[argc - i];
                    wordCount = 0;
                }
                words[wordCount] = new char[alen + 1];
                strcpy(words[wordCount], arg);
                if (replaceRandom) {
                    for(int j = 0 ; j < alen ; j++) {
                        words[wordCount][j] = tolower(words[wordCount][j]);
                    }
                }
                wordCount++;
            } else {
                wordsIgnored++;
            }
        }
    }
    
    // MASTER OPTIONS FOR RESUME - check dir & arguments from file
    if (argDir) {
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(argDir)) != NULL) {
            /* print all the files and directories within directory */
            while ((ent = readdir(dir)) != NULL) {
                const char* fname = ent->d_name;
                if (0 != strcmp(fname, ".") && 0 != strcmp(fname, "..")) {
                    int frk = fork();
                    if (frk == 0) {
                        // child !
                        string arg1 = "-a";
                        // fixme - add path
                        arg1 += argDir;
                        if (arg1.c_str()[arg1.length() - 1] != '/') {
                            arg1 += "/";
                        }
                        arg1 += fname;
                        char* const newArgs[] = { (char*)argv[0], (char*)arg1.c_str(), nullptr };
                        if (execvp(argv[0], newArgs) < 0) {
                            std::cerr << "Could not resume due to execvp fail." << std::endl;
                            cleanup();
                            return -1;
                        };
                    } else
                    if (fork < 0) {
                        std::cerr << "Could not fork to open file." << std::endl;
                        cleanup();
                        return -1;
                    }
                }
            }
            closedir (dir);
        } else {
            /* could not open directory */
            std::cerr << "Could not open directory: " << argDir << std::endl;
            cleanup();
            return -1;
        }
        return 0;
    }
    
    if (argFile) {
        if (anyOption) {
            std::cerr << "Resuming from file excludes any other options." << std::endl;
            cleanup();
            return -1;
        }
        // reinvoke itself by reading arguments from file
        struct stat fstat;
        if (0 != stat(argFile, &fstat)) {
            std::cerr << "Resume file not accessible: " << argFile << std::endl;
            cleanup();
            return -1;
        }
        FILE* inf = fopen(argFile, "r");
        if (!inf) {
            std::cerr << "Resume file not accessible for open: " << argFile << std::endl;
            cleanup();
            return -1;
        }
        char* buffer = new char[fstat.st_size + 1];
        memset(buffer, 0, fstat.st_size + 1);
        ssize_t read = fread(buffer, 1, fstat.st_size, inf);
        fclose(inf);
        if (read == fstat.st_size) {
            string str = "";
            vector<string> args;
            args.push_back(argv[0]);
            for(ssize_t i = 0; i < read + 1 ; i++) {
                char c = buffer[i];
                if( c == ' ' || c == 10 || c == 0){
                    if (str.length() > 0) {
                        args.push_back(str.c_str());
                        str = "";
                    }
                } else if(c == '\"' ){
                    i++;
                    while( i < read && buffer[i] != '\"' ) {
                        str += buffer[i];
                        i++;
                    }
                } else {
                    str += c;
                }
            }
            delete[] buffer;
            // now args... for execvp
            size_t argl = args.size();
            const char** newArgs = new const char* [argl + 1];
            for(size_t i = 0 ; i < argl ; i++) {
                newArgs[i] = args.at(i).c_str();
            }
            newArgs[argl] = nullptr;
            if (execvp(argv[0], (char * const *)newArgs) < 0) {
                std::cerr << "Could not resume due to execvp fail." << std::endl;
                cleanup();
                return -1;
            };
            return 0;
        }
        delete[] buffer;
        std::cerr << "Resume file could not be read: " << argFile << std::endl;
        cleanup();
        return -1;
    }
    
    // check benchmarking
    if (benchmark) {
        if (anyOption) {
            std::cerr << "Benchmarking excludes any other options." << std::endl;
            cleanup();
            return -1;
        }
        charset.push_back(benchmarkCharset);
        outFile = benchmarkOutput;
    }
    // option supercede
    if (replaceRandom) {
        if (replaceFirst) {
            std::cerr << "* WARNING * -u supercedes -c" << std::endl;
            replaceFirst = false;
        }
        if (toUpper) {
            std::cerr << "* WARNING * -u supercedes -tu" << std::endl;
            toUpper = false;
        }
        if (toUpper) {
            std::cerr << "* WARNING * -u supercedes -tl" << std::endl;
            toLower = false;
        }
    }
    // translate replace chars to charlist
    replaceCount = (int)replacev.size();
    if (replaceCount > 0) {
        replaceList = new char*[replaceCount];
        for(int i = 0 ; i < replaceCount ; i++) {
            char* rr = replacev[i];
            replaceList[i] = rr;
        }
    }
    replacev.clear();
    // check resumeData vs resumeAt
    if (resumeAt > 0 && resumeData) {
        std::cerr << "-x and -z cannot be used together." << std::endl;
        cleanup();
        return -1;
    }
    // check minChar and maxChar
    if (minChar <= 0) {
        minChar = defaultMinChar;
    }
    if (maxChar >= 0 && maxChar < minChar) {
        std::cerr << "Max characters must be more or equal to min characters." << std::endl;
        cleanup();
        return -1;
    }
    // final check
    if (minChar < ignoreShorterThan) {
        std::cerr << "Options -i and -min given contraindicatory values." << std::endl;
        cleanup();
        return -1;
    }
    // check once more, since here we could have problem
    if (ignoreLongerThan <= 0) {
        ignoreLongerThan = ignoreLongerDefault;
    }
    if (ignoreShorterThan > ignoreLongerThan) {
        std::cerr << "-i and -I given contradictory values." << std::endl;
        return -1;
    }
    // check input
    std::cerr << "Reading input." << std::endl;
    if (inFile.size() > 0) {
        char buffer[256];
        std::vector<char*> vec;
        buffer[255] = 0;
        for(std::vector<const char*>::iterator it = inFile.begin() ; it != inFile.end(); ++it) {
            // open file
            ifinput.open(*it);
            if (!ifinput.is_open()) {
                std::cerr << "Could not open file for reading: " << *it << std::endl;
                cleanup();
                return -1;
            }
            // read words if appropriate
            while (ifinput.getline(buffer, 255)) {
                // do something with the word
                int wlen = (int)strlen(buffer);
                if (wlen > 0) {
                    if (wlen >= ignoreShorterThan && wlen <= ignoreLongerThan) {
                        char* word = new char[wlen + 1];
                        strcpy(word, buffer);
                        if (replaceRandom) {
                            for(int j = 0 ; j < wlen ; j++) {
                                word[j] = tolower(word[j]);
                            }
                        }
                        vec.push_back(word);
                    } else {
                        wordsIgnored++;
                    }
                }
            }
            // close input
            ifinput.close();
        }
        int newWordCount = (int)vec.size();
        if (newWordCount > 0) {
            char** newWords = new char*[wordCount + newWordCount];
            if (words) {
                for(int j = 0 ; j < wordCount ; j++) {
                    newWords[j] = words[j];
                }
                delete[] words;
            }
            for(int j = 0 ; j < newWordCount ; j++) {
                newWords[wordCount + j] = vec[j];
            }
            words = newWords;
            wordCount += newWordCount;
            vec.clear();
        }
    }
    // check pyrit readiness && options
    if (capFile) {
        struct stat buf;
        if (stat(capFile, &buf) != 0) {
            std::cerr << "File does not exists: " << capFile << std::endl;
            cleanup();
            return -1;
        }
        if (!outFile && separatedChildOutput) {
            std::cerr << "-S cannot be used without -w." << std::endl;
            cleanup();
            return -1;
        }
        // tweaks for OCL HASHCAT
        if (childToUse == USE_OCLHASHCAT) {
            if (useBlocking) {
                std::cerr << "-B is always on with -h." << std::endl;
            } else {
                useBlocking = true;
            }
            if (childInstances > 1) {
                std::cerr << "-P cannot be used with -h." << std::endl;
                cleanup();
                return -1;
            }
            if (separatedChildOutput) {
                std::cerr << "-S cannot be used with -h." << std::endl;
                cleanup();
                return -1;
            }
        }
    } else {
        if (useBlocking) {
            std::cerr << "-B cannot be used without -p or -h." << std::endl;
            cleanup();
            return -1;
        }
    }
    if (capFile && childInstances < 1) {
        childInstances = defChildInstances;
    }
    // check and prepare output
    if (outFile) {
        if (capFile) {
            // check conflicts
            if (childInstances < 1 && separatedChildOutput) {
                std::cerr << "Cannot use separated outputs with one child instance (-S and -p/-h)" << std::endl;
                cleanup();
                return -1;
            }
            // if capFile then just check if the file exists
            // remove backup if exists
            for(int i = 0 ; i < childInstances ; i++) {
                char* newOutFile = new char[1024];
                char *backFile = new char[1024];
                if (separatedChildOutput) {
                    sprintf(newOutFile, "%s-%d", outFile, i+1);
                    sprintf(backFile, "%s-%d~", outFile, i+1);
                } else {
                    sprintf(newOutFile, "%s", outFile);
                    sprintf(backFile, "%s~", outFile);
                }
                // don't care about errors here
                unlink(backFile);
                // move old to backup
                struct stat fex;
                if (0 == stat(newOutFile, &fex)) {
                    // file exists
                    if (0 != rename(newOutFile, backFile)) {
                        std::cerr << "Could not backup output file: " << newOutFile << std::endl;
                        cleanup();
                        return -1;
                    }
                }
                delete[] backFile;
                // now check opening of output file
                FILE* check = fopen(newOutFile, "w");
                if (!check) {
                    std::cerr << "Could not open file for writing: " << newOutFile << std::endl;
                    delete[] newOutFile;
                    cleanup();
                    return -1;
                }
                fclose(check);
                delete[] newOutFile;
            }
        } else {
            ofoutput.open(outFile);
            if (!ofoutput.is_open()) {
                std::cerr << "Could not open file for writing: " << outFile << std::endl;
                cleanup();
                return -1;
            }
            output = &ofoutput;
        }
    } else {
        output = &std::cout;
    }
    // check update file
    if (oFile) {
        // check if exists and writable
        FILE* check = fopen(oFile, "a");
        if (!check) {
            std::cerr << "Could not open update file for writing: " << oFile << std::endl;
            cleanup();
            return -1;
        } else {
            fclose(check);
        }
    }
    // if charset, prepare word list
    for(std::vector<const char*>::iterator it = charset.begin() ; it != charset.end(); ++it) {
        int charSetLen = (int)strlen(*it);
        char** newWords = new char*[wordCount + charSetLen];
        if (words) {
            for(int i = 0; i < wordCount ; i++) {
                newWords[i] = words[i];
            }
            delete[] words;
        }
        char repeated = 0;
        for(int i = 0 ; i < charSetLen ; i++) {
            char* word = new char[2];
            word[0] = (*it)[i];
            word[1] = 0;
            newWords[i + wordCount] = word;
            // check repeats
            for(int j = 0 ; j < i ; j++) {
                if ((*it)[j] == (*it)[i]) {
                    repeated = (*it)[j];
                    break;
                }
            }
        }
        words = newWords;
        wordCount += charSetLen;
        if (repeated != 0) {
            std::cerr << "Repeating character (" << repeated << ") given for charset: " << *it << std::endl;
            cleanup();
            return -1;
        }
    }
    // prepare word list - check for repeats, go to lower or uppercase
    if (toUpper || toLower) {
        std::cerr << "Transforming to proper case." << std::endl;
        for(int i = 0 ; i < wordCount ; i++) {
            char* wrd = words[i];
            int wlen = (int)strlen(wrd);
            for(int j = 0 ; j < wlen ; j++) {
                wrd[j] = toLower ? tolower(wrd[j]) : toUpper ? toupper(wrd[j]) : wrd[j];
            }
        }
    }
    if (reduceRepeats) {
        std::cerr << "Parsing wordlist and reducing repeating pattern, this may last long." << std::endl;
        std::vector<const char*> wordList;
        for(int i = 0 ; i < wordCount ; i++) {
            wordList.push_back(words[i]);
        }
        std::cerr << "Sorting." << std::endl;
        std::sort(wordList.begin(), wordList.end(), sorter);
        std::vector<const char*> newWordList;
        const char* lastWord = nullptr;
        for(std::vector<const char*>::iterator it = wordList.begin() ; it != wordList.end(); ++it) {
            const char* currentWord = *it;
            //std::cerr << "W=" << currentWord << std::endl;
            if (lastWord == nullptr || 0 != strcmp(currentWord, lastWord)) {
                newWordList.push_back(currentWord);
            } else {
                wordsIgnored++;
            }
            lastWord = currentWord;
        }
        int nwCount = (int)newWordList.size();
        if (nwCount <= 0) {
            std::cerr << "Not enough words to continue." << std::endl;
            cleanup();
            return -1;
        }
        std::cerr << "Rewriting wordlist." << std::endl;
        char** newWords = new char*[nwCount];
        nwCount = 0;
        for(std::vector<const char*>::iterator it = newWordList.begin() ; it != newWordList.end(); ++it) {
            newWords[nwCount++] = (char*)*it;
        }
        delete[] words;
        words = newWords;
        wordCount = nwCount;
    }
    // word count
    std::cerr << "Word count: " << wordCount << std::endl;
    // calculate min and max len
    std::cerr << "Calculating minimum and maximum word length" << std::endl;
    for(int i = 0 ; i < wordCount ; i++) {
        char* wrd = words[i];
        int wlen = (int)strlen(wrd);
        if (minLength < 0 || wlen < minLength) {
            minLength = wlen;
        }
        if (maxLength < 0 || wlen > maxLength) {
            maxLength = wlen;
        }
    }
    // check word list
    if (wordCount < 1 || (wordCount < 2 && !replaceFirst && !replaceRandom && !replaceList)) {
        std::cerr << "Not enough words to mix." << std::endl;
        cleanup();
        return -1;
    }
    if (wordsIgnored > 0) {
        std::cerr << "* WARNING : " << wordsIgnored <<  " words have been ignored (too long, too short or repeating)"<< std::endl;
    }
    // adjust numPositions
    if (minNumPositions < 0) {
        minNumPositions = 1;
    }
    if (maxNumPositions > 0) {
        numPositions = maxNumPositions;
    } else {
        // calculate number of positions
        numPositions = (minChar / minLength) + (minChar % minLength == 0 ? 0 : 1);
        // if no repeat, need to strip number of positions
        if (norepeat && numPositions > wordCount) {
            numPositions = wordCount;
        }
    }
    // see if this match
    if (minNumPositions > numPositions) {
        std::cerr << "Minimum number of positions is greater than maximum. Consider using -l and -L." << std::endl;
        cleanup();
        return -1;
    }
    // adjust maxChar
    if (maxChar <= 0) {
        maxChar = numPositions * maxLength;
    }
    // check maxChar and numPositions
    if (maxChar < minChar) {
        std::cerr << "Cannot generate output, minimum required character count is " << minChar << " but the maximum achievable phrase length for " << numPositions << " positions is " << maxChar << ". Consider using -min." << std::endl;
        cleanup();
        return -1;
    }
    // need to sort via length
    std::cerr << "Preparing word length tables" << std::endl;
    std::vector<WordDef> lwords;
    for(int i = 0 ; i < wordCount ; i++) {
        WordDef wd;
        wd.word = words[i];
        wd.index = i;
        lwords.push_back(wd);
    }
    std::sort(lwords.begin(),lwords.end(),lengther);
    // now we need to have words sorted out by length
    // maxLength is the maximum
    wordsByLengthNumber = maxChar + 1;
    wordsByLength = new int*[wordsByLengthNumber];
    wordsByLengthCount = new int[wordsByLengthNumber];
    vector<int> curwords;
    for(int i = 0 ; i <= maxChar ; i++) {
        wordsByLength[i] = nullptr;
        wordsByLengthCount[i] = 0;
    }
    int currentLen = (int)strlen((*lwords.begin()).word);
    // last word is null so it can (and should) be skipped
    WordDef empty;
    empty.word = nullptr;
    empty.index = -1;
    lwords.push_back(empty);
    for(std::vector<WordDef>::iterator it = lwords.begin() ; it != lwords.end(); ++it) {
        const char* word = (*it).word;
        int index = (*it).index;
        int len = word ? (int)strlen(word) : 0;
        // if step changed or last step; on last step, word is null
        if (len > currentLen || !len) {
            // transfer curwords to proper table
            int tlen = (int)curwords.size();
            int* lenwords = new int[tlen];
            int j = 0;
            for(std::vector<int>::iterator iit = curwords.begin() ; iit != curwords.end(); ++iit) {
                lenwords[j++] = *iit;
            }
            // substitute table
            wordsByLength[currentLen] = lenwords;
            wordsByLengthCount[currentLen] = j;
            // prepare vector for next step
            curwords.clear();
            currentLen = len;
        }
        curwords.push_back(index);
    }
    // just a warning
    if (maxChar - minChar > 16) {
        std::cerr << "* WARNING * minimum character number is " << minChar << " while maximum is " << maxChar << "." << std::endl;
        std::cerr << "            consider using -min and/or -max" << std::endl;
    }
    // write starting communication
    std::cerr << "Mix will now start working..." << std::endl;
    // also try to setup pyrit
    if (capFile) {
        switch(childToUse) {
            case USE_OCLHASHCAT:
                std::cerr << "Starting oclHashcat command" << std::endl;
                if (!providedPath) {
                    providedPath = defHashcatPath;
                }
                break;
            case USE_PYRIT:
                std::cerr << "Starting pyrit command" << std::endl;
                if (!providedPath) {
                    providedPath = defPyritPath;
                }
                break;
            default:
                std::cerr << "Could not open child, unrecognized type " << childToUse << std::endl;
                cleanup();
                return -1;
        }
        poutput = new FILE*[childInstances];
        dpoutput = new int[childInstances];
        dpolloutput = new struct pollfd[childInstances];
        dpollcount = 0;
        char buffer[1024];
        for(int i = 0 ; i < childInstances ; i++) {
            switch(childToUse) {
                case USE_OCLHASHCAT:
                    if (outFile) {
                        if (useChildWriteOption) {
                            sprintf(buffer, hashcatwOutputToFile, providedPath, outFile, capFile);
                        } else {
                            sprintf(buffer, hashcatwOutput, providedPath, capFile, outFile);
                        }
                    } else {
                        sprintf(buffer, hashcat, providedPath, capFile);
                    }
                    break;
                case USE_PYRIT:
                    if (outFile) {
                        if (separatedChildOutput) {
                            sprintf(buffer, useChildWriteOption ? pyritwSeparatedOutputToFile : pyritwSeparatedOutput, providedPath, capFile, outFile, i+1);
                        } else {
                            sprintf(buffer, useChildWriteOption ? pyritwOutputToFile : pyritwOutput, providedPath, capFile, outFile);
                        }
                    } else {
                        sprintf(buffer, pyrit, providedPath, capFile);
                    }
                    break;
            }
            poutput[i] = popen(buffer, "w");
            if (!poutput[i]) {
                std::cerr << "Could not open child with pipe." << std::endl;
                cleanup();
                return -1;
            }
            dpoutput[i] = fileno(poutput[i]);
            if (!useBlocking) {
                fcntl(dpoutput[i], F_SETFL, O_NONBLOCK);
                dpolloutput[i].fd = dpoutput[i];
                dpolloutput[i].events = POLLOUT;
            }
        }
        std::cerr << "Setting " << (useBlocking ? "" : "non-") << "blocking mode." << std::endl;
    } else {
        if (childInstances > 0) {
            std::cerr << "Cannot use -P with no -p set." << std::endl;
            cleanup();
            return -1;
        }
        if (useChildWriteOption) {
            std::cerr << "Cannot use -W with no -p set." << std::endl;
            cleanup();
            return -1;
        }
    }
    if  (resumeAt > 0) {
        std::cerr << "Resuming at line: " << resumeAt << ", please wait..." << std::endl;
    }
    if (benchmark) {
        std::cerr << " *** BENCHMARKING FOR " << benchmarkDuration <<  " SECONDS ***" << std::endl;
    }
    // pre-mix word list according to options
    std::cerr << "Pre-mixing, this may last long." << std::endl;
    premix();
    if (quit) {
        cleanup();
        return -1;
    }
    // preparing stuff
    wordUsage = new int[numPositions];
    resetCounters();
    int cMaxChar = maxChar;
    int startChar = minChar;
    // try to read previous data
    if (resumeData) {
        int ti = 0;
        vector<string> tokens;
        char* tok = strtok((char*)resumeData, "@");
        while(tok) {
            tokens.push_back(tok);
            tok = strtok(nullptr, "@");
            ti++;
            if (ti > numPositions + 1) {
                std::cerr << "Resume data format is bad (number of positions wrong - too many); cannot continue." << std::endl;
                cleanup();
                return -1;
            }
        }
        if (ti != 1 + numPositions) {
            std::cerr << "Resume data format is bad (number of positions wrong - not enough); cannot continue." << std::endl;
            cleanup();
            return -1;
        }
        for(int ti = 0 ; ti < numPositions + 1 ; ti++)
        {
            // tok is token
            tok = (char*)tokens.at(ti).c_str();
            int sti = 0;
            vector<int> values;
            char* subtok = strtok(tok, ":#");
            while(subtok) {
                // token is subtok
                int val = atoi(subtok);
                values.push_back(val);
                // next subtoken
                subtok = strtok(nullptr, ":#");
                sti++;
            }
            // use token and subtokens
            if (ti == 0) {
                // minChar and others
                int vcnt = (int)values.size();
                if (vcnt < 2) {
                    std::cerr << "Resume data format is bad (first block not correct); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                int savedWordCount = values.at(0);
                if (savedWordCount != wordCount) {
                    std::cerr << "Resume data format is bad (word count is not the same); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                int mcc = 0;
                if (vcnt != 3) {
                    std::cerr << "Resume data format is bad (subblock count not correct); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                resumeAtPattern = values.at(1);
                mcc = values.at(2);
                if (mcc < minChar || mcc > maxChar) {
                    std::cerr << "Resume data format is bad (minChar declaration bad); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                startChar = mcc;
            } else {
                int vsize = (int)values.size();
                if (vsize < 1 || vsize > 1 + numPositions) {
                    std::cerr << "Resume data format is bad (subwords positions not correct); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                int wrd = values.at(0);
                if (wrd < -1 || wrd >= wordCount) {
                    std::cerr << "Resume data format is bad (word count exceeded); cannot continue." << std::endl;
                    cleanup();
                    return -1;
                }
                // still not enought data
                wordUsage[ti - 1] = wrd;
                if (wrd >= 0) {
                    WordDesc* wd = &dictionary[wrd];
                    for(int i = 0 ; i < numPositions ; i++) {
                        wd->lastUsed[i] = vsize > i + 1 ? values.at(i + 1) : 0;
                    }
                }
            }
        }
    }
    // produce patterns
    std::cerr << "Generating patterns." << std::endl;
    producePatterns(minChar, maxChar);
    if (quit) {
        cleanup();
        return -1;
    }
    // check patterns
    if (producedPatterns.size() <= 0) {
        std::cerr << "No valid patterns found; cannot continue." << std::endl;
        cleanup();
        return -1;
    }
    // final checks
    // ** THIS WAS COPIED FROM BELOW
    if (wordCount < 1 || (wordCount < 2 && !replaceFirst && !replaceRandom && !replaceList)) {
        std::cerr << "Not enough words to mix." << std::endl;
        cleanup();
        return -1;
    }
    if (wordsIgnored > 0) {
        std::cerr << "* WARNING : " << wordsIgnored <<  " words have been ignored (too long, too short or repeating)"<< std::endl;
    }
    // adjust numPositions
    if (maxNumPositions > 0) {
        numPositions = maxNumPositions;
    } else {
        // calculate number of positions
        numPositions = (minChar / minLength) + (minChar % minLength == 0 ? 0 : 1);
        // if no repeat, need to strip number of positions
        if (norepeat && numPositions > wordCount) {
            numPositions = wordCount;
        }
    }
    // adjust maxChar
    if (maxChar <= 0) {
        maxChar = numPositions * maxLength;
    }
    // check maxChar and numPositions
    if (maxChar < minChar) {
        std::cerr << "Cannot generate output, minimum required character count is " << minChar << " but the maximum achievable phrase length for " << numPositions << " positions is " << maxChar << ". Consider using -min." << std::endl;
        cleanup();
        return -1;
    }
    // ***** DOWN TO HERE
    // start mixing and writing
    just_print();
    if (!noWork) {
        // start time counter
        startTime = (long)time(nullptr);
        // do mixing
        for(int c = startChar ; c <= cMaxChar ; c++) {
            minChar = c;
            maxChar = c;
            pattern();
            if (quit) {
                break;
            } else {
                resetCounters();
                resumeAtPattern = 0;
            }
        }
    }
    // write final data stats
    write_data();
    // cleanup & exit
    cleanup();
    // exit
    if (!quit) {
        std::cerr << "Clean exit" << std::endl;
        if (oFile) {
            remove(oFile);
        }
    }
    return 0;
}

void resetCounters() {
    normalFinish = false;
    for(int i = 0 ; i < numPositions ; i++) {
        wordUsage[i] = -1;
    }
    for(int i = 0 ; i < wordCount ; i++) {
        WordDesc* wd = &dictionary[i];
        for(int j = 0 ; j < numPositions ; j++) {
            wd->lastUsed[j] = 0;
        }
    }
}

void just_print() {
    allWordCount = 0;
    unsigned long allWordBytes = 0;
    unsigned long long totalMixedWords = 0;
    for(int i = minChar ; i <= maxChar ; i++) {
        int wblc = wordsByLengthCount[i];
        int* wordsToUse = wordsByLength[i];
        for (int j = 0 ; j < wblc ; j++) {
            int realWord = wordsToUse[j];
            WordDesc* wd = &dictionary[realWord];
            totalMixedWords += wd->subWordCount;
        }
    }
    double avLength = 0.0;
    std::vector<std::vector<int>> patterns = getPatterns(minChar,maxChar);
    int ps = (int)patterns.size();
    for(int i = 0 ; i < ps ; i++) {
        std::vector<int> pattern = patterns[i];
        int ptr = (int)pattern.size();
        unsigned long long patternWords = 1;
        unsigned long long patternBytes = 0;
        int patternLen = 0;
        for(int j = 0; j < ptr ; j++) {
            int tlen = pattern[j];
            patternLen += tlen;
            int* wordsToUse = wordsByLength[tlen];
            int maxValues = wordsByLengthCount[tlen];
            // iterate ?
            unsigned long long positionWordCount = 0;
            for(int k = 0 ; k < maxValues ; k++) {
                int realWord = wordsToUse[k];
                WordDesc* wd = &dictionary[realWord];
                positionWordCount += wd->subWordCount;
            }
            // positionWordCount will mix with patternWords
            patternWords *= positionWordCount;
        }
        allWordCount += patternWords;
        patternBytes += patternLen * patternWords;
        allWordBytes += patternBytes;
    }
    avLength = (double)allWordBytes / (double)allWordCount;
    std::cerr << "Total mixed word count: " << totalMixedWords << ", number of word positions to mix: " << numPositions << std::endl;
    std::cerr << "Minimum word length: " << minLength << ", maximum word length: " << maxLength << ", average word length: " << avLength << std::endl;
    unsigned long long xlines = allWordCount;
    std::cerr << "Calculated count of generated words: " << xlines << std::endl;
    xlines /= 1000;
    if (xlines > 1) {
        std::cerr << xlines << " kwords" << std::endl;
    }
    xlines /= 1000;
    if (xlines > 1) {
        std::cerr << xlines << " mwords" << std::endl;
    }
    xlines /= 1000;
    if (xlines > 1) {
        std::cerr << xlines << " gwords" << std::endl;
    }
    xlines /= 1000;
    if (xlines > 1) {
        std::cerr << xlines << " twords" << std::endl;
    }
    xlines /= 1000;
    if (xlines > 1) {
        std::cerr << xlines << " pwords" << std::endl;
    }
    std::cerr << allWordBytes << " bytes to generate" << std::endl;
    double kbytes = allWordBytes / 1024;
    if (kbytes > 1.0) {
        std::cerr << kbytes << " KB" << std::endl;
    }
    double mbytes = kbytes / 1024;
    if (mbytes > 1.0) {
        std::cerr << mbytes << " MB" << std::endl;
    }
    double gbytes = mbytes / 1024;
    if (gbytes > 1.0) {
        std::cerr << gbytes << " GB" << std::endl;
    }
    double tbytes = gbytes / 1024;
    if (tbytes > 1.0) {
        std::cerr << tbytes << " TB" << std::endl;
    }
    double pbytes = tbytes / 1024;
    if (pbytes > 1.0) {
        std::cerr << pbytes << " PB" << std::endl;
    }
    if (cpersec > 0) {
        unsigned long long secs = allWordCount / cpersec;
        std::cerr << "At given speed of " << cpersec << " operations per second, this will last " << secs << " seconds," << std::endl;
        unsigned long long mins = secs / 60;
        unsigned long long hours = mins /60;
        unsigned long long days = hours / 24;
        double years = days > 365 ? days / 365.25 : 0.0;
        if (years > 0.0) {
            long ly = (long)years;
            std::cerr << "In other words, " << ly << " years, ";
            if (ly < 100) {
                long mind = (long)((double)365.25 * (double)60);
                secs = secs - (ly * mind * 60 * 24);
                mins = secs / 60;
                hours = mins / 60;
                days = hours / 24;
                if (days > 31) {
                    long months = days / 30;
                    std::cerr << months << " months, " << days % 30 << " days and some more hours" << std::endl;
                } else {
                    std::cerr << days << " days, " << hours << " hours" << std::endl;
                }
            } else {
                if (ly > 1000) {
                    std::cerr << " or " << ly/1000 << " thousand years";
                }
                std::cerr << std::endl;
            }
        } else {
            secs = secs - (days * 24 * 60 * 60);
            hours = secs / 3600;
            secs = secs - (hours * 3600);
            mins = secs / 60;
            secs = secs - (mins * 60);
            std::cerr << "In other words, " << days << " days, " << hours << " hours, " << mins << " minutes and " << secs << " seconds" << std::endl;
        }
    }
}

void premix() {
    dictionary = new WordDesc[wordCount];
    for(int i = 0 ; i < wordCount && !quit ; i++) {
        WordDesc* wd = &dictionary[i];
        const char* word = words[i];
        // generate mix TODO
        std::vector<const char*> newWV;
        newWV.push_back(word);
        if (replaceFirst) {
            char origin = word[0];
            char target = toupper(origin);
            if (origin != target) {
                char* capitalized = new char[strlen(word) + 1];
                strcpy(capitalized,word);
                capitalized[0] = target;
                newWV.push_back(capitalized);
            }
        }
        if (downFirst) {
            char origin = word[0];
            char target = tolower(origin);
            if (origin != target) {
                char* decapitalized = new char[strlen(word) + 1];
                strcpy(decapitalized,word);
                decapitalized[0] = target;
                newWV.push_back(decapitalized);
            }
        }
        if (replaceRandom) {
            int len = (int)strlen(word);
            for(long x = 1 ; x < (1 << len) ; x++) {
                bool changed = false;
                char* randomized = new char[len + 1];
                strcpy(randomized, word);
                for(int y = 0 ; y < len ; y++) {
                    long test = 1 << y;
                    char origin = word[y];
                    char newvalue = test & x ? toupper(origin) : origin;
                    randomized[y] = newvalue;
                    changed = changed || origin != newvalue;
                }
                if (changed) {
                    newWV.push_back(randomized);
                } else {
                    delete[] randomized;
                }
            }
        }
        if (replaceList) {
            // new vector, to have always clean base in newWV
            std::vector<const char*> newNewWV;
            for(std::vector<const char*>::iterator it = newWV.begin() ; it != newWV.end() && !quit ; ++it) {
                const char* wword = *it;
                int len = (int)strlen(wword);
                for(long x = 1 ; x < (1 << len) ; x++) {
                    bool changed = true;
                    char* randomized = new char[len + 1];
                    strcpy(randomized, wword);
                    for(int y = 0 ; y < len && changed ; y++) {
                        long test = 1 << y;
                        char origin = wword[y];
                        char newvalue = origin;
                        if (test & x) {
                            // test which position is applicable
                            for(int j = 0 ; j < replaceCount ; j++) {
                                if (replaceList[j][0] == origin) {
                                    newvalue = replaceList[j][1];
                                    break;
                                }
                            }
                            changed = changed && origin != newvalue;
                        }
                        randomized[y] = newvalue;
                    }
                    if (changed) {
                        // final check
                        bool found = false;
                        for(std::vector<const char*>::iterator iit = newNewWV.begin() ; iit != newNewWV.end() ; ++iit) {
                            if (0 == strcmp(*iit, randomized)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            newNewWV.push_back(randomized);
                        } else {
                            delete[] randomized;
                        }
                    } else {
                        delete[] randomized;
                    }
                }
            }
            // then append new vector to current
            for(std::vector<const char*>::iterator it = newNewWV.begin() ; it != newNewWV.end(); ++it) {
                newWV.push_back(*it);
            }
        }
        
        // end of generate mix
        int swc =(int)newWV.size();
        wd->subwords = new const char*[swc];
        int guard = 0;
        for(std::vector<const char*>::iterator it = newWV.begin() ; it != newWV.end(); ++it) {
            wd->subwords[guard++] = *it;
        }
        // +1 stands for original
        wd->subWordCount = swc;
        // prepare to use the first one
        wd->lastUsed = new int[numPositions];
        for(int j = 0 ; j < numPositions ; j++) {
            wd->lastUsed[j] = j == 0 ? -1 : 0;
        }
    }
}

int lastWriteIndex = -1;
unsigned long lastLinesWritten = 0;
unsigned long lastMeanPerSec = 0;
time_t lastScreenUpdate = 0;

void push(char* buffer, int len) {
    if (resumeAt <= linesWritten) {
        bytesWritten += len;
        if (poutput) {
            // calculate number of pipe for output
            int toWrite = -1;
            if (useBlocking) {
                int pipeNo = linesWritten % childInstances;
                toWrite = dpoutput[pipeNo];
            } else {
                // if more descriptors were ready, reuse them
                if (dpollcount <= 0) {
                    // -1 is for timeout infinite
                    dpollcount = poll(dpolloutput, childInstances, -1);
                }
                // Check if any descriptors ready
                if (dpollcount == -1 || dpollcount == 0) {
                    // report error and abort, ==0 should never happen
                    std::cerr << "Child input terminated. Pipe closed." << std::endl;
                    quit = true;
                } else {
                    // now we have number of descriptors ready
                    int selectorIndex = lastWriteIndex;
                    for(int i = 0 ; i < childInstances ; i++) {
                        selectorIndex = (selectorIndex + 1) % childInstances;
                        if (dpolloutput[selectorIndex].revents & POLLOUT) {
                            dpolloutput[selectorIndex].revents = 0;
                            toWrite = dpoutput[selectorIndex];
                            lastWriteIndex = selectorIndex;
                            dpollcount--;
                            break;
                        }
                    }
                }
            }
            if (!quit && (write(toWrite, buffer, len) < 0 || write(toWrite, "\n", 1) < 0)) {
                std::cerr << "Child input terminated. Pipe closed." << std::endl;
                quit = true;
            }
        } else {
            output->write(buffer, len);
            output->write("\n", 1);
        }
        if (oFile && linesWritten % oFileCycle == 0) {
            // time to write update
            FILE* update = fopen(oFile, "w");
            if (update) {
                // get -z data
                std::string ret = getResumeData();
                // resume at beginning, with space, since we expect arguments
                fprintf(update, "-z%s ", ret.c_str());
                // skip 1-st argument as this is program itself
                bool ignoreNext = false;
                for(int i = 1 ; i < argNum ; i++) {
                    const char* nxtArg = argVal[i];
                    size_t len = strlen(nxtArg);
                    bool isZ = strncmp(nxtArg, "-z", 2) == 0;
                    if (!isZ && !ignoreNext) {
                        fwrite(nxtArg, len, 1, update);
                        fwrite(" ", 1, 1, update);
                    }
                    ignoreNext = isZ && len == 2;
                }
                fclose(update);
            } else {
                std::cerr << "Something bad happened. Cannot write update file: " << oFile << std::endl;
                quit = true;
            }
        }
    }
    linesWritten++;
    // screen update - once per 1000 lines or per sec
    if (linesWritten % 1000 == 0) {
        time_t currentTime = time(nullptr);
        if (currentTime > lastScreenUpdate) {
            lastScreenUpdate = currentTime;
            unsigned long meanPerSec = linesWritten - lastLinesWritten;
            lastMeanPerSec = (lastMeanPerSec + meanPerSec) / 2;
            lastLinesWritten = linesWritten;
            char* current = new char[len + 1];
            memcpy(current, buffer, len);
            current[len] = 0;
            std::cerr << "\33[2K\rTotal: " << linesWritten << " PMKs, stream: " << lastMeanPerSec << " PMKs/sec, current: " << current;
            delete[] current;
        }
    }
    // now general update
    if (show) {
        show = false;
        char* toShow = new char[len + 1];
        memcpy(toShow, buffer, len);
        toShow[len] = 0;
        std::cerr << std::endl << "Currently processing: " << toShow << "    " << std::endl;
        std::string ret = getResumeData();
        std::cerr << "Fast resume data (use -z<data> for resume): " << ret.c_str() << std::endl;
        std::cerr << "Complete line to resume is:" << std::endl << argVal[0] << " -z" << ret.c_str() << " ";
        bool ignoreNext = false;
        for(int i = 1 ; i < argNum ; i++) {
            const char* nxtArg = argVal[i];
            size_t len = strlen(nxtArg);
            bool isZ = strncmp(nxtArg, "-z", 2) == 0;
            if (!isZ && !ignoreNext) {
                std::cerr << nxtArg << " ";
            }
            ignoreNext = isZ && len == 2;
        }
        std::cerr << std::endl << std::endl;
    }
    if (quitAt >=0 && quitAt <= linesWritten) {
        quit = true;
    } else {
        if (benchmark && time(nullptr) >= startTime + benchmarkDuration) {
            quit = true;
        }
    }
}

void producePatterns(int minLen, int maxLen) {
    producedPatterns.clear();
    // first pattern - one int, size of minChar to maxChar
    if (minNumPositions <= 1) {
        for(int i = minLen ; i <= maxLen ; i++) {
            if (wordsByLengthCount[i] > 0) {
                std::vector<int> oneWord;
                oneWord.push_back(i);
                producedPatterns[i].push_back(oneWord);
            }
        }
    }
    // now we need from 2 (at least) to numPositions other patterns
    int startNumPositions = minNumPositions > 1 ? minNumPositions : 2;
    for(int np = startNumPositions ; np <= numPositions && !quit ; np++) {
        // prepare positions table
        int* positions = new int[np];
        // minimum length value is always 1
        int minValue = 1;
        for(int i = 0 ; i < np ; i++) {
            positions[i] = minValue;
        }
        // calculate maximum position value
        int maxValue = maxChar - (minValue * (np - 1));
        bool mov = false;
        do {
            // check sum of positions
            int sum = 0;
            for(int i = 0 ; i < np ; i++) {
                int request = positions[i];
                if (wordsByLengthCount[request] > 0) {
                    sum += request;
                } else {
                    sum = 0;
                    break;
                }
            }
            if (sum >= minLen && sum <= maxLen) {
                vector<int> aword;
                for(int i = 0 ; i < np ; i++) {
                    int request = positions[i];
                    aword.push_back(request);
                }
                producedPatterns[sum].push_back(aword);
            }
            // add 1 to first position check for ov
            int i = 0;
            bool overflow = false;
            do {
                if (i == 0 || overflow) {
                    overflow = false;
                    if (++positions[i] > maxValue) {
                        overflow = true;
                        positions[i] = minValue;
                        if (i == np - 1) {
                            mov = true;
                        }
                    }
                }
                if (mov || ++i >= np) {
                    break;
                }
            } while(overflow && !mov && !quit);
        } while(!mov);
        delete[] positions;
    }
}

std::vector<std::vector<int>> getPatterns(int minLen, int maxLen) {
    std::vector<std::vector<int>> ret;
    for(int i = minLen ; i <= maxLen ; i++) {
        std::vector<std::vector<int>> subret = getPatterns(i);
        int srs = (int)subret.size();
        for(int j = 0 ; j < srs ; j++) {
            ret.push_back(subret[j]);
        }
    }
    return ret;
}

std::vector<std::vector<int>> getPatterns(int forLen) {
    return producedPatterns[forLen];
}

void pattern() {
    // wordsByLength[requested length] has now sorted word list
    // prepare patterns - this may be on vectors, since its rarely used
    vector<vector<int>> patterns = getPatterns(minChar, maxChar);
    // check if any patterns :)
    int numPatterns = (int)patterns.size();
    if (numPatterns <= 0) {
        return;
    }
    // now the patterns are here, time to mixxxx
    for ( ; resumeAtPattern < numPatterns && !quit ; resumeAtPattern++) {
        std::vector<int> pattern = patterns[resumeAtPattern];
        int psize = (int)pattern.size();
        // remove this log (if necessary)
        unsigned long long wcount = 1;
        std::cerr << std::endl << "Processing pattern: [";
        for(int i = 0 ; i < psize ; i++) {
            int tlen = pattern[i];
            int* wordsToUse = wordsByLength[tlen];
            int maxValues = wordsByLengthCount[tlen];
            // iterate ?
            unsigned long long positionWordCount = 0;
            for(int k = 0 ; k < maxValues ; k++) {
                int realWord = wordsToUse[k];
                WordDesc* wd = &dictionary[realWord];
                positionWordCount += wd->subWordCount;
            }
            // positionWordCount will mix with patternWords
            wcount *= positionWordCount;
            std::cerr << tlen;
            if (i < psize - 1) {
                std::cerr << ",";
            }
        }
        unsigned long long upto = ((unsigned long long)linesWritten + wcount);
        int percent = (int)((100 * upto) / allWordCount);
        std::cerr << "], word count: " << wcount << " (up to " << upto << " total or " << percent << "%) " << std::endl;
        // END OF REMOVE
        if (wcount <= 0) {
            // just in case, should never happen
            continue;
        }
        const char** selected = new const char*[psize];
        int* used = new int[psize];
        for(int i = 0 ; i < psize ; i++) {
            int val = 0;
            int usage = wordUsage[i];
            if (usage >= 0) {
                // usage is real-word index
                // need to translate into oer-length index
                int tlen = pattern[i];
                int* wordsToUse = wordsByLength[tlen];
                int maxValues = wordsByLengthCount[tlen];
                int j;
                for(j = 0 ; j < maxValues ; j++) {
                    int indexedWord = wordsToUse[j];
                    if (indexedWord == usage) {
                        val = j;
                        break;
                    }
                }
                if (j >= maxValues) {
                    // nothing found !
                    std::cerr << "* WARNING * Error on resume, cannot find indexed value." << std::endl;
                }
            }
            used[i] = val;
        }
        int buflen = maxChar + 1;
        char* buffer = new char[buflen];
        bool megaOverflow = false;
        while(!quit && !megaOverflow) {
            // then get words
            bool repeats = false;
            bool overflow = false;
            for(int i = 0 ; i < psize ; i++) {
                // usage is index in digits
                int tlen = pattern[i];
                int* wordsToUse = wordsByLength[tlen];
                int maxValues = wordsByLengthCount[tlen];
                int usage = used[i];
                int realWord = wordsToUse[usage];
                // update usage
                wordUsage[i] = realWord;
                // check repeats
                if (!repeats) {
                    for(int j = 0 ; j < i ; j++) {
                        if (realWord == wordUsage[j]) {
                            repeats = true;
                            break;
                        }
                    }
                }
                // prepare word to push
                WordDesc* wd = &dictionary[realWord];
                int lastUsed = wd->lastUsed[i];
                selected[i] = wd->subwords[lastUsed];
                // lowest position or overflow
                if (i == 0 || overflow) {
                    overflow = false;
                    if (++wd->lastUsed[i] >= wd->subWordCount) {
                        wd->lastUsed[i] = 0;
                        if (++used[i] >= maxValues) {
                            overflow = true;
                            used[i] = 0;
                            realWord = wordsToUse[0];
                            wordUsage[i] = realWord;
                            // mega overflow ?
                            if (i == psize - 1) {
                                megaOverflow = true;
                            }
                        }
                    }
                }
            }
            // check for quit
            if (quit) {
                break;
            }
            // check for repeats
            if (repeats && norepeat) {
                continue;
            }
            // assembly phrase
            int pos = 0;
            for(int i = 0 ; i < psize ; i++) {
                int wlen = (int)strlen(selected[i]);
                memcpy(&buffer[pos],selected[i],wlen);
                pos += wlen;
            }
            // write-out
            push(buffer, minChar);
        }
        delete[] buffer;
        delete[] used;
        delete[] selected;
        // check quit
        if (quit) {
            break;
        }
        // prepare for next loop
        if (resumeAtPattern < numPatterns - 1) {
            resetCounters();
        }
    }
    if (resumeAtPattern >= numPatterns && !quit) {
        normalFinish = true;
    }
}

void cleanup() {
    if (poutput) {
        for(int i = 0 ; i < childInstances ; i++) {
            if (poutput[i]) {
                pclose(poutput[i]);
                poutput[i] = nullptr;
            }
        }
        delete[] poutput;
        poutput = nullptr;
    }
    if (dpolloutput) {
        delete[] dpolloutput;
        dpolloutput = nullptr;
    }
    if (dpoutput) {
        delete[] dpoutput;
        dpoutput = nullptr;
    }
    if (outFile && output && ofoutput.is_open()) {
        ofoutput.close();
        output = nullptr;
    }
    if (replaceList) {
        for(int i = 0 ; i < replaceCount ; i++) {
            delete[] replaceList[i];
        }
        delete[] replaceList;
        replaceList = nullptr;
        replaceCount = 0;
    }
    if (dictionary) {
        for(int i = 0 ; i < wordCount ; i++) {
            WordDesc& wd = dictionary[i];
            for(int j = 1 ; j < wd.subWordCount ; j++) {
                delete[] wd.subwords[j];
            }
            delete[] wd.subwords;
            delete[] wd.lastUsed;
        }
        delete[] dictionary;
        dictionary = nullptr;
    }
    if (wordUsage) {
        delete[] wordUsage;
    }
    if (wordsByLength) {
        for(int i = 0 ; i < wordsByLengthNumber ; i++) {
            delete[] wordsByLength[i];
        }
        delete[] wordsByLength;
    }
    if (wordsByLengthCount) {
        delete[] wordsByLengthCount;
    }
    if (words) {
        for(int i = 0 ; i < wordCount ; i++) {
            if (words[i]) {
                delete[] words[i];
            }
        }
        delete[] words;
        words = nullptr;
        wordCount = 0;
    }
}


