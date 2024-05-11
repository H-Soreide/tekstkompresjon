#!/bin/bash

# Sjekk om riktig mengde parametre er gitt
# eksempel: './run.sh ./icu-pakk hound.txt out.z stats/ ../statistikkanalyse/statistikker/new 20 30 20 50 20 80'
if [ $# -lt 6 ]; then
    echo "Usage: $0 [program_path] [innputfile] [outputfile] [stats_folder] [newstats_folder] [cutoff1] [hyppig1] [cutoff2] [hyppig2] ..."
    exit 1
fi

program_path=$1
innputfile=$2
outputfile=$3
stats_folder=$4
newstats_folder=$5

# Sjekk om programmet er gyldig
if [ ! -x "$program_path" ]; then
    echo "Error: Program '$program_path' not found or not executable."
    exit 1
fi

shift 5

outsizelist=()
input_list=("$@")

input_list=("$@")

# Sjekk om cutoff og hyppighet kommer i par
if [ $(( ${#input_list[@]} % 2 )) -ne 0 ]; then
    echo "Error: The input list must have an even number of elements."
    exit 1
fi

# kjør programmet med alle gitt cutoffs og hyppig
for (( i=0; i<${#input_list[@]}; i+=2 )); do
    cutoff=${input_list[i]}
    hyppig=${input_list[i+1]}

    echo "Running program '$program_path' with parameters '$innputfile' '$outputfile' and cutoff: $cutoff", hyppig: $hyppig

    "$program_path" -9 "$innputfile" "$outputfile" "$cutoff" "$hyppig"

    outsizelist+=($(stat --printf="%s" "$outputfile"))

done

echo "Output file size (in bytes): "
echo "${outsizelist[@]}"

# flytt alle statistikk filene til newstats_folder 
cp -a "$stats_folder/." "$newstats_folder"
