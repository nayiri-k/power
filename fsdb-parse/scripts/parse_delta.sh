#!/bin/bash
file=output.delta
line=`tail -n 1 $file`
max_num_words=$(echo "$line" | cut -d ' ' -f 1- | wc -w)

beginning=true

string="0.000000"
tail -n +3 $file |  grep -v '^$' | while read line
do
    # echo $line
    num_words=$(echo "$line" | cut -d ' ' -f 1- | wc -w)

    if $beginning; then
        if [ $num_words -lt $max_num_words ]; then
            n=$(($max_num_words - $num_words))
            addon=`printf " %0.s$string" $(seq 1 $n)`
            line=${line}${addon}
        fi
    elif [ $num_words -lt $max_num_words ]; then
        # we reached max line length
        beginning=false
    fi

    echo $line
done