# mixer
A wrapper around pyrit WPA cracker; resumable multiple dictionaries mixer with charset support

*** LINUX NOT FULLY SUPPORTED ! *** right now, linux'es popen does not support open for RW,
therefore, opening pipe on Linux does NOT work.

* WARNING * I know this code is ugly and looks like s**t. BUT, it does what it was supposed to,
and if I get enough requests for refactoring - I will make it out into a beautiful C++ project.

******* WHAT IT IS ********

Given the famous pyrit program, one will always face problem with finding appropriate dictionaries
for it. There are some online English (and other) dictionaries, but for successful WPA
security testing it is simply not enought.

Most of crackable Wifi passwords are dictionary-based, i.e. they consist of two or more
combined parts, which may be separate words, or numbers. Simple dictionary attack will not
succeed for passwords like 'internetpassw0rd'...

Yes, there is 'crunch' program. But its options are not sufficient and, moreover, crunch
cannot resume its operation :)


**** LEGAL WARNING ****

Unauthorised wireless security testing is a legal offence and may lead you into real problems.
Be sure to use this software ONLY when You are allowed to try to penetrate wireless security.


******* RUNNING MIX *******

At the end of this article, you will find a printout of help, also accessible when invoking mix with no options.


MOST TYPICAL USAGE:

  mix -pcaptured.cap -l1 -L4 -max16 -oresume.res -fdictionary1.txt -fdictionary2.txt -s0123456789 -c -Ro0 -wresult.txt

This will read captured packets from captured.cap file, mix one to four words from dictionaries dictionary1.txt and dictionary2.txt,
add mixing of characters 0123456789, try to capitalize first letter of each dictionary word, try to replace o with 0
in each word that has o inside, startup pyrit crunching with attack_passthrough option and write result to result.txt.
Maximum password length will be 16 letters. A resume file resume.res will be created and updated each 1000000 tries.

MOST TYPICAL ERROR:

  mix -max14 -s01234567890

... and expecting maximum word length of 14 characters. Since mixer will mix WORDS only, and by default, minimum number
of characters is 8 and of words is 1, therefore, only 8-letter words will be generated. With mixing of charset, try
to use -L option:

  mix -L14 -s0123456789

**** LICENSE ****

Use it freely, and provide Your feedback. Use this software only with accordance to Your local law.


**** STANDARD HELP ****

    mix [options] <word1> <word2> [word3] ....

            (ctrl-c on running application will produce benchmarks)


    Options:
    
    -min<number>     - minimum number of characters (default 8)
    
    -max<number>     - maximum number of characters (recommended to use this)
    
    -s<charset>      - use provided charset
    
    -f<path>         - read basic wordset from file instead of stdin
    
    -w<path>         - write generated wordset to file instead of stdout, cannot be used with -p
    
    -r<char1><char2> - generate more words by replacing char1 with char2 (case-sensitive)
    
    -R<char1><char2> - generate more words by replacing char1 with char2 (case-insensitive)
    
    -l<number>       - minimum number of mixed words (recommended to use this)
    
    -L<number>       - maximum number of mixed words (recommended to use this)
    
    -i<number>       - ignore words shorter than certain number of characters (1 by default). If set and -min is not used,
                     it will also override -min default setting.
                     
    -I<number>       - ignore words longer than certain number of characters (64 by default if -max
                     is not used, if -max is used, then by default equal to -max; -I supercedes -max)
                     
    -n               - do not repeat words in phrase
    
    -tl              - pre-process all words to lowercase
    
    -tu              - pre-process all words to uppercase
    
    -c               - generate more words by capitalizing first letter of the word
    
    -d               - generate more words by decapitalizing first letter of the word
    
    -u               - generate more words by permutatively capitalizing all letters of the word (supercedes -c).
                     This will also lowercase all the words given. Use with caution.
                     
    -e               - eliminate repeating words, this may take a very long time
    
    -x<number>       - resume at specific line number, cannot be used with -z.
    
    -z<data>         - resume at specific program state, fast, but requires special data format, excludes -x.
    
    -q<number>       - quit at specific line number
    
    -b               - run benchmark only for 10 seconds, this excludes all other options
    
    -v[number]       - number specifies operations per second; this option will give additional
    
    -p<path>         - path to .cap file, this will pipe pyrit command with parameters:
                       pyrit -r <path> -i - attack_passthrough
                       
    -g<path>         - complete path to pyrit, if relative does not work
    
    -a<path>         - read arguments from file, cannot be used with other arguments
    
    -h<dir path>     - read all files from given directory and invoke this program with -a<path>
                     for each file found, this option cannot be used with any other options
                     
    -o<path>         - write regular update of state to given file, this file can be used with -a option
    
    -O<number>       - regular update write in each of <number> cycles; this option requires -o<path>
                     if not specified, then 1000000 is used

                     Note: use SIGUSR1 to trigger printout of current word.


