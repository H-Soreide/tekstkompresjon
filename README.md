## About
This repository contains the work done during IDATT2900 Bacheloroppgave. The program uses libicu to analyze a text document, 
and uses arithmetic coding to create a compressed word list, and saves this word list and statistic models describing the text to an output file.
The program also runs a simulation that aims to calculate how many bits per symbol are needed for the compression.
The initial code was supplied by our supervisor Helge Hafting.

## Dependencies

- linux
- c++ compiler
- [libicu](https://icu.unicode.org/)

## Compile

```
git clone https://github.com/H-Soreide/tekstkompresjon.git
cd tekstkompresjon
make
```

Note: The ```Makefile``` is dependant on libicu's makefile, which can be installed in different directories. If ```make``` fails, try changing the include statement in ```Makefile``` to match your systems libicu location

## Run
To run the program with default cutoff values there are three required arguments.

```
./icu-pakk [-1..9] innput output
```

The first number argument describes how much debug information is printed.

- 7 : wordlist
- 8 : all sentances
- 9 : every word

You can also supply custom cutoff values for follow stats and position stats.

```
./icu-pakk [-1..9] innput output <word_cutoff> <frequency_cutoff>
```
- word_cutoff : any positive integer
- frequency_cutoff : any integer from 0 - 100, representing 0% - 100%.

## run.sh
To run the program multiple times automaticly with different cutoff values, you can use the ```run.sh``` script.

```
Usage: ./run.sh program_path innputfile outputfile stats_folder newstats_folder [word-cutoff1 frequency-cutoff1] [word-cutoff2 frequency-cutoff2] ...
```

- program_path : path of program executable
- innputfile : file to be compressed
- outputfile : output representing compressed file
- stats_folder : path where statistic files are written
- newstats_folder : copies generated statistics to this folder
- [word-cutoff1 frequency-cutoff1] : pair of cutoffs to run the program with, multiple pairs can be supplied

## Examples
Run the program with hound.txt as innput, default cutoffs.
```
./icu-pakk -9 hound.txt out.z
```

Run the program with hound.txt as innput, and word cutoff of 20 and frequency cutoff of 80%.
```
./icu-pakk -9 hound.txt out.z 20 80
```

Run the program three times with word cutoff of 10, 100 and 1000.
```
./run.sh ./icu-pakk hound.txt out.z stats/ .. 10 100 100 100 1000 100
```
