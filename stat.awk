#!/usr/bin/awk
NR==1 { 
    OFS="\t"
    max=$1
    min=$1 
    sum=0
    ssm=0
}
{ 
    if ($1>max) max=$1
    if ($1<min) min=$1
    sum += $1
    ssm += $1*$1
}
END {
    if (NR>0) {
        avg = sum/NR
        print ">>> stat:", NR, min, max, avg, sqrt(ssm/NR-avg*avg)
    } else {
        print ""
    }
}

