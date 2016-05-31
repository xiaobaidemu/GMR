#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <stdlib.h> 
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdio.h> 
#include <string.h> 
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h> 
#include "mpi.h"
#include "error.h"
#include "partition.h"
#include "gmr.h"
#include "algorithms.h"

using namespace std;

/** 运行示例:
 * 1.)单机运行: mpirun -np pro_num ./gmr algorithm partition graphfile
 * 2.)集群运行: mpirun -mahcinefile hosts ./gmr algorithm partition graphfile
 */
int main(int argc, char *argv[]) {
    /* rank, size: MPI进程序号和进程数, sb: 发送缓存, rb: 接收缓存 */
    int rank, size, i, rbsize = 0, sbsize = 0;
    Edge *sb = nullptr;
    Edge *rb = nullptr;
    graph_t graph;
    GMR *gmr = nullptr;
    std::list<KV> reduceResult;
    char default_graph[256] = "graph/small.graph";

    /* 初始化MPI */
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    double starttime = MPI_Wtime();
    /* allothersendcounts: 
     * 接收所有进程发送数据的大小, 用于判断迭代结束(若所有进程都无数据需要发送)
     * sendcountswithconv:带收敛进度的发送缓存区大小(image: sendcounts : conv) 
     * sendcounts, recvcounts: 数据发送和接收缓冲区
     * sdispls, rdispls: 发送和接收数据缓冲区的偏移 */
    int *allothersendcounts = (int*)malloc((size + 1) * size * sizeof(int));
    int *sendcountswithconv = (int*)malloc((size + 1) * sizeof(int));
    int *sendcounts         = (int*)malloc(size * sizeof(int));
    int *recvcounts         = (int*)malloc(size * sizeof(int));
    int *sdispls            = (int*)malloc(size * sizeof(int));
    int *rdispls            = (int*)malloc(size * sizeof(int));

    /* 如果没有提供任何命令行参数, 则采用默认算法和分图 */
    if (argc == 1 && rank == 0) {
            printf("===>示例本地运行(采用rdsmall.graph图文件和随机分图算法)<===\n");
    }
    /* 参数提供的顺序: mpirun -np 3 gmr algorithm partition graphfile */
    /* 参数个数大于4时, 显示程序的调用格式 */
    if (argc > 3) {
        printf("Usage:1.)mpirun -np 3 ./gmr graphfile random\n"
                 "2.)mpirun -machinefile hosts -np 3 ./gmr graphfile metis\n");
        exit(0);
    }
    
    if (argc > 2) {
        if (checkfileexist(argv[2]))
            read_input_file(rank, size, argv[2], &graph);
        else {
            MPI_Finalize();
            exit(0);
        }
    }
    else
        read_input_file(rank, size, default_graph, &graph);

    printf("Process %d: G(|V|, |E|) = (%d, %d)\n", rank, graph.nvtxs,
            graph.nedges);
    /* 根据调用命令行实例化相应算法, 没用提供则模式使用TriangeCount算法 */
    /* 如果有提供算法名字, 则使用规定的算法 */
    if (argc > 1) {
        if (strcmp(argv[1], "pagerank") == 0)
            gmr = new PageRank();
        else if (strcmp(argv[1], "trianglecount") == 0)
            gmr = new TriangleCount();
        else if (strcmp(argv[1], "sssp") == 0)
            gmr = new SSSP(1);
        else {
            printf("目前没有提供%s的图算法方法.\n", argv[1]);
            exit(0);
        }
    }
    else
        gmr = new TriangleCount();

    /* 将子图的顶点个数赋给进程的全局变量 */
    ntxs = graph.nvtxs;
    if(INFO) printf("%d 节点和边数: %d %d\n", rank, graph.nvtxs,
            graph.xadj[graph.nvtxs]);
    if (DEBUG) displayGraph(&graph);

    /* 首先使用算法对图进行初始化:
     * PageRank: 初始化为空
     * SSSP: 如果调用的是SSSP算法, 需要将图进行初始化
     * startv.value = 0, otherv = ∞ */
    gmr->initGraph(&graph);

    while(true && iterNum < MAX_ITERATION && iterNum < gmr->algoIterNum){
        /* 获取发送数据的大小, 并将其放到发送缓冲区 */
        /* 从当前子图获取需要向其他节点发送的字节数 */
        getSendBufferSize(&graph, size, rank, sendcounts);
        memset(allothersendcounts, 0, (size + 1) * size * sizeof(int));
        memset(sendcountswithconv, 0, (size + 1) * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        /* 打印出节点发送缓冲区大小 */
        if(INFO) {
            printf("Process %d send size:", rank);
            for (i = 0; i < size; i++) {
                printf(" %d\t", sendcounts[i]);
            }
            printf("\n");
        }
        
        /* 计算迭代进度(收敛顶点数/总的顶点数 * 10,000), 并将其加在缓存大小后面
         * ,接收数据后,首先判断所有进程的收敛进度是否接收,再拷贝接收缓存大小 */
        recordTick("bexchangecounts");
        /* 将收敛精度乘以10000, 实际上是以精确到小数点后两位以整数形式发送 */
        int convergence = 10000;
        if (graph.nvtxs > 0) 
            convergence = (int)(1.0 * convergentVertex / graph.nvtxs * 10000);
        memcpy(sendcountswithconv, sendcounts, size * sizeof(int));
        memcpy(sendcountswithconv + size, &convergence, sizeof(int));
        /* 交换需要发送字节数和收敛的进度 */
        MPI_Allgather(sendcountswithconv, size + 1, MPI_INT, allothersendcounts,
                size + 1, MPI_INT, MPI_COMM_WORLD); 
        i = size;
        while(allothersendcounts[i] == 10000 && i < (size + 1) * size) i += (size + 1);
        if (i > (size + 1) * size - 1) break;
        /* 将本节点接收缓存区大小拷贝到recvcounts中 */
        for (i = 0; i < size; i++)
            recvcounts[i] = allothersendcounts[rank + i * (size + 1)];
        recordTick("eexchangecounts");

        /* 申请发送和接收数据的空间 */
        rbsize = accumulate(recvcounts, recvcounts + size, 0);
        sbsize = accumulate(sendcounts, sendcounts + size, 0);
        totalRecvBytes += rbsize * sizeof(Edge);
        rb = (Edge*)calloc(sizeof(Edge), rbsize);
        if ( !rb ) {
            perror( "can't allocate recv buffer");
            MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        sb = (Edge*)calloc(sizeof(Edge), sbsize);
        if ( !sb ) {
            perror( "can't allocate send buffer");
            free(rb); MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        /* 计算发送和接收缓冲区偏移 */
        for (i = 1; i != size; i++) {
            sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
        recordTick("bgetsendbuffer");
        if (sbsize > 0) getSendbuffer(&graph, sdispls, size, rank, sb);
        recordTick("egetsendbuffer");

        /* 打印出节点接收到的接收缓冲区大小 */
        if(INFO) {
            printf("Prcess %d recv size:", rank);
            for (i = 0; i < size; i++) {
                printf(" %d\t", recvcounts[i]);
            }
            printf("\nProess %d send vertexs:", rank);
            if (DEBUG)
            for (i = 0; i < size; i++) {
                for (int j = sdispls[i]; j < sdispls[i] + sendcounts[i]; j++) {
                    Edge edge;
                    memcpy(&edge, sb + j, sizeof(Edge));
                    printf("->%d(%d, %d, %f, %f) ", i,edge.vid, edge.fvid, edge.fwgt, edge.fewgt);
                }
            }
        }

        /* 调用MPIA_Alltoallv(), 交换数据 */
        recordTick("bexchangedata");
        for (int i = 0; i < size; i++)
            sendcounts[i] *= sizeof(Edge), sdispls[i] *= sizeof(Edge),
            recvcounts[i] *= sizeof(Edge), rdispls[i] *= sizeof(Edge);
        MPI_Alltoallv((void*)sb, sendcounts, sdispls, MPI_BYTE,
                (void*)rb, recvcounts, rdispls, MPI_BYTE, MPI_COMM_WORLD);
        recordTick("eexchangedata");

        /* 将交换的数据更新到树上 */
        sort(rb, rb + rbsize, edgeComp); 
        recordTick("bupdatepre");
        updateGraph(&graph, rb, rbsize, rank);
        recordTick("eupdatepre");

//         printf("Graph==>%d:", rank);
//         for (i = 0; i < graph.nvtxs; i++) {
//             printf("(");
//             for (int k = graph.prexadj[i]; k < graph.prexadj[i + 1]; k++) {
//                 printf("%d ", graph.preadjncy[k]);
//             }
//             printf(")-->");
//             printf(" %d-->(", graph.ivsizes[i]);
//             for (int j = graph.xadj[i]; j < graph.xadj[i + 1]; j++) {
//                     printf(" %d@%d ", graph.adjncy[j], graph.adjloc[j]);
//             }
//             printf(")  ");
//         }
//         printf("\n");

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        recordTick("bcomputing");
        computing(rank, &graph, (char*)rb, rbsize, gmr, reduceResult); 
        recordTick("ecomputing");
        
        // 释放rb, sb
        if (rbsize > 0) free(rb), rbsize = 0;
        if (sbsize > 0) free(sb), sbsize = 0;

        /* 将最终迭代的结果进行更新到子图上, 并判断迭代是否结束 */
        recordTick("bupdategraph");
        if (reduceResult.size() > 0) updateGraph(rank, &graph, reduceResult, gmr->upmode);
        reduceResult.clear();
        recordTick("eupdategraph");

        MPI_Barrier(MPI_COMM_WORLD);
        recordTick("eiteration");

        iterNum++;
        printTimeConsume(rank);
    }
    printf("程序运行结束,总共耗时:%f secs, 通信量:%ld Byte, 最大消耗"
            "内存:(未统计)Byte\n", MPI_Wtime() - starttime, totalRecvBytes);

    MPI_Finalize();
    /* 打印处理完之后的结果(图) */
    //displayGraph(graph);
    gmr->printResult(&graph);
    graph_Free(&graph);
    if (sendcounts) free(sendcounts);
    if (sdispls) free(sdispls); if(rdispls) free(rdispls);
    if (recvcounts) free(recvcounts);
    if(allothersendcounts) free(allothersendcounts);
    if(sendcountswithconv) free(sendcountswithconv);
}
