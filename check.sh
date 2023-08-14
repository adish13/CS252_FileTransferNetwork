fld=$1
n=$2
p=$3

for ((i=1 ; i<=$n ; i++))
do
    echo "----------------------------------"
    echo $i
    echo "----------------------------------"
    diff -Bw ./output/op-c$i-p$p.txt `printf '%s/ans_%s_%s.txt' $fld $i $p`
    echo "----------------------------------"

done
