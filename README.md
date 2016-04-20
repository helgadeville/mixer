# mixer
A wrapper around pyrit/oclHashcat [WPA] crackers; resumable multiple dictionaries mixer with charset support

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

*** A NOTE ON OCLHASHCAT INSTALLATION ***

oclHashcat is distributed as a zip containing executable and some subfolders. Mixer will try to invoke
"oclHashcat" command, which is NOT part of oclHashcat package.
I have found out, that the best way to install oclHashcat into Linux system is to copy the whole package
into /usr/local/bin, i.e. the contents of the package are in /usr/local/bin/oclHashcat-2.01 folder.
Then, easy enough is to create a short script /usr/local/bin/oclHashcat:

    #!/bin/sh
    /usr/local/bin/oclHashcat-2.01/oclHashcat64.bin "$@"
    
This will enable system-wide command "oclHashcat". Note: ln -s ... does not work in this case.

**** STANDARD HELP ****

This program will generate a mix from given word list
    (version 0.6)

 Usage:

    mix [options] <word1> <word2> [word3] ....

            (ctrl-c on running application will produce benchmarks)


     Options: (please note that you may separate values from option or concatenate them)
      -min<number>     - minimum number of characters (default 8)
      -max<number>     - maximum number of characters (recommended to use this)
      -s<charset>      - use provided charset
      -f<path>         - read basic wordset from file instead of stdin
      -w<path>         - write generated wordset to file instead of stdout, if used with -p or -h, then pyrit/oclHashcat output will be written to file
      -W<path>         - same as -w, except it will use write option of pyrit/oclHashcat instead of stdout redirection to file; must be used with -p or -h
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
      -P<number>       - number of instances of pyrit to run, default 1; use only with -p<>.
                         Cannot be used with -h
      -h<path>         - path to .hccap file, this will pipe oclHashcat command with parameters:
                           oclHashcat -m 2500 --workload-profile=3 <path>
                         This will also use blocking output (-B)
      -S               - separate pyrit outputs when -w is used. This option requires -P and -w options.
                         Cannot be used with -h
      -B               - Use blocking output to pyrit/oclHashcat. This option requires -p or -h option.
                         With -h, it is on by default.
      -g<path>         - complete path to pyrit/oclHashcat, if relative does not work
      -a<path>         - read arguments from file, cannot be used with other arguments
      -h<dir path>     - read all files from given directory and invoke this program with -a<path>
                         for each file found, this option cannot be used with any other options
      -o<path>         - write regular update of state to given file, this file can be used with -a option
      -O<number>       - regular update write in each of <number> cycles; this option requires -o<path>
                         if not specified, then 1000000 is used
    
                         Note: use SIGUSR1 to trigger printout of current word.


