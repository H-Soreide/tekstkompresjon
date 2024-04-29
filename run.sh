#!/bin/bash

# Sjekk om riktig mengde parametre er gitt
# eksempel: './run.sh ./icu-pakk hound.txt out.z stats/ ../statistikkanalyse/statistikker/new 20 40 60 80 100 120 140'
if [ $# -lt 6 ]; then
    echo "Usage: $0 [program_path] [innputfile] [outputfile] [stats_folder] [newstats_folder] [number1] [number2] ..."
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

# kjør programmet med alle gitt cutoffs
for number in "$@"; do
    echo "Running program '$program_path' with parameters '$innputfile' '$outputfile' and number: $number"
    "$program_path" -9 "$innputfile" "$outputfile" "$number"
done

# flytt alle statistikk filene til newstats_folder 
cp -a "$stats_folder/." "$newstats_folder"
