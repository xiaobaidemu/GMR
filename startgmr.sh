#!/bin/bash
#

# 节点个数, 从machinefile中获取
command="mpirun"
hostsfile="hosts"
hosts_num=0 
algorithm="pagerank"
graphfile="rdmdual.graph"
partition="random"

if [ $# -lt 1 ]; then
    echo "执行默认测试算法和示例数据"
    mpirun -np 3 ./igmr
    exit 0
fi

# 判断集群运行, 还是单节点运行
if [ "$1" = "cluster" ]; then
    echo "cluster"
    if [ $# -lt 2 ]; then
        echo "./startgmr.sh cluster hosts algorithm partition graphfile"
        exit 0
    fi
    if [ $# -gt 1 ]; then
        hostsfile=$2
        command="$command"" -machinefile $hostsfile"
        # 判断提供的hostsfile文件是否存在
        if [ ! -f "$hostsfile" ]; then
            echo "提供的machinefile不存在"
            exit 0
        fi
        # 遍历hosts文件中的节点个数
        for i in `grep -v "#" $2`; do
            hosts_num=`expr $hosts_num + 1`
        done
        command="$command"" -np $hosts_num"
    fi
    command=$command" ./igmr "
    if [ $# -gt 2 ]; then
        algorithm=$3
        command="$command"" $algorithm"
    fi
    if [ $# -gt 3 ]; then
        partition=$4
        command="$command"" $partition"
    fi
    if [ $# -gt 4 ]; then
        graphfile=$5
        command="$command"" $graphfile"
    fi
else
    # ... ./gmr algorithm partition graphfile
    command=$command" -np 3 igmr "
    if [ $# -gt 0 ]; then
        algorithm=$1
        command="$command"" $algorithm"
    fi
    if [ $# -gt 1 ]; then
        partition=$2
        command="$command"" $partition"
    fi
    if [ $# -gt 2 ]; then
        graphfile=$3
        command="$command"" $graphfile"
    fi
fi
echo "执行命令:"$command
eval $command
