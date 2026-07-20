#!/bin/bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 RESULT_DIRECTORY" >&2
    exit 2
fi

result_root=$1
combined=$result_root/combined.tsv
summary=$result_root/summary.tsv
failures=$result_root/failures.tsv

first=1
for result_file in "$result_root"/*.tsv; do
    [[ -f $result_file ]] || continue
    case $(basename "$result_file") in
        combined.tsv|summary.tsv|failures.tsv) continue ;;
    esac
    if (( first )); then
        head -1 "$result_file" >"$combined"
        first=0
    fi
    tail -n +2 "$result_file" >>"$combined"
done

if (( first )); then
    echo "no per-case TSV files found in $result_root" >&2
    exit 1
fi

{
    printf 'platform\ttree\tstack\tnetwork\tallocate\tstrided\tvector\tatomicity\tflush_request_atomics\tresult\tcount\n'
    awk -F '\t' 'BEGIN {OFS="\t"}
        NR > 1 {count[$1 FS $2 FS $3 FS $4 FS $11 FS $12 FS $13 FS $14 FS $15 FS $7]++}
        END {for (key in count) print key, count[key]}' "$combined" | sort
} >"$summary"

awk -F '\t' 'NR == 1 || $7 != "PASS"' "$combined" >"$failures"

echo "combined=$combined"
echo "summary=$summary"
echo "failures=$failures"
awk -F '\t' 'NR > 1 {count[$7]++} END {for (result in count) print result, count[result]}' "$combined" | sort
