/*************************************************************************
 > File Name: testsamplesort.cpp
 > Author: Weidong, ZHANG
 > Mail: wdfnst@pku.edu.cn
 > Created Time: 四  2/18 12:06:28 2016
 ************************************************************************/
#include <iostream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cstdio>
#include <list>

#include "mpi.h"
#include "testsamplesort.h"

using namespace std;

#define DEBUG true

int main(int argc, char *argv[]) {
    int quantityofnumber = 0;
    int rank, size, i, j, root = 0;
    char filename[256];
    
    /* 初始化MPI */
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    /* 每个Processor根据样本的分隔符对自己数据分段之后, 需要发往其他的数据信息*/
    int *sendcounts = (int*)malloc(size * sizeof(int));
    int *recvcounts = (int*)malloc(size * sizeof(int));
    int *sdispls    = (int*)malloc(size * sizeof(int));
    int *rdispls    = (int*)malloc(size * sizeof(int));
    int *sbuf       = (int *)malloc((size - 1) * sizeof(int)); 
    int *rbuf       = (int *)malloc(size * (size - 1) * sizeof(int)); 
    int *allothersendcounts = (int*)malloc(size * size * sizeof(int));

    /* Step-1: 为每个processor分配n/p个元素 */
    sprintf(filename, "testdata/number.part.%d", rank);
    ifstream fin(filename);
    while(fin.good()) {
        fin >> i;
        if (fin.tellg() > 0)
            quantityofnumber++;
    }
    fin.close();
    int *numbers = (int*)malloc(quantityofnumber * sizeof(int));
    ifstream fin1(filename);
    j = 0;
    while(fin1.good()) {
        fin1 >> i;
        if (fin1.tellg() > 0)
            numbers[j++] = i;
    }
    cout << endl;
    fin1.close();

    /* Step-2: 每个processor对自己分配的元素排序 */
    sort(numbers, numbers + quantityofnumber);
    if (DEBUG) {
        cout << "Process " << rank << " after sort(:" << quantityofnumber << ")\n";
        for (i = 0; i < quantityofnumber; i++)
            cout << numbers[i] << " ";
        cout << endl;
    }

    /* Step-3: 每个processor从自己分配的元素中选出p-1个分隔元素,
     * 将已排序元素分隔为平等的p段 */
    int sectionSize = quantityofnumber / size;
    memset(sbuf, 0, (size - 1) * sizeof(int));
    memset(rbuf, 0, size * (size - 1) * sizeof(int));
    if (DEBUG) cout << "partition:";
    for (i = 1, j = 0; i < quantityofnumber; i++) {
        if (i % sectionSize == 0 && j < size - 1) {
            if (DEBUG) cout << numbers[i] << "\t";
            sbuf[j++] = numbers[i];
        }
    }
    if (DEBUG) cout << endl;

    /* Step-4: 将每个processor选出的分隔元素（共有p(p-1)个）合并，
     * 再从中选出p-1个分隔元素，将这p(p-1)个元素平分为p段 */
    MPI_Gather(sbuf, size - 1, MPI_INT, rbuf, size - 1, MPI_INT, root, MPI_COMM_WORLD);
    list<int> tempilist;
    if (rank == root) {
        for (i = 0; i < size * (size - 1); i++) 
            tempilist.push_back(rbuf[i]);
        tempilist.sort();
        if (DEBUG) {
            cout << "分段符:\n";
            for (auto iter = tempilist.begin(); iter != tempilist.end(); iter++)
                cout << *iter << " ";
            cout << endl;
        }

        memset(sbuf, 0, (size - 1) * sizeof(int));
         i = 1, j = 0;
        for (auto iter = tempilist.begin(); iter != tempilist.end(); iter++, i++) {
            if (i % (size - 1) == 0 && j < size - 1) {
                sbuf[j++] = *iter;
            }
        }
        if (DEBUG) {
            cout << "分割p(p-1)的(p-1)的分段符:";
            for (i = 0; i < size - 1; i++)
                cout << sbuf[i] << "\t";
            cout << endl;
        }
    }

    /* Step-5: 以上一步选出的p-1个元素为界限，每个processor分配给自己的元素划分为p段 */
    if (rank != root) memset(sbuf, 0, (size - 1) * sizeof(int));
    MPI_Bcast(sbuf, size * (size - 1), MPI_INT, root, MPI_COMM_WORLD );
    if (DEBUG) {
        cout << "Process " << rank << " 接收到的样本分隔符:";
        for (i = 0; i < size - 1; i++)
            cout << sbuf[i] << "\t";
        cout << endl;
    }
    /* 根据接收到的样本分隔符对本地数据进行分割 */
    memset(sendcounts, 0, size * sizeof(int));
    for (i = 0, j = 0; i < size - 1; i++) {
        for (; j < quantityofnumber; j++) {
            if (numbers[j] <= sbuf[i])
                sendcounts[i]++;
            else
                break;
        }
    }
    sendcounts[size - 1] = quantityofnumber;
    for (i = 0; i < size -1; i++)
        sendcounts[size - 1] -= sendcounts[i];
    /* 交换每个进程需要发送的整数个数 */
    memset(allothersendcounts, 0, size * size * sizeof(int));
    MPI_Allgather(sendcounts, size, MPI_INT, allothersendcounts,
            size, MPI_INT, MPI_COMM_WORLD); 
    for (i = 0; i < size; i++)
        recvcounts[i] = allothersendcounts[rank + i * size];
    /* 计算发送和接收缓冲区偏移 */
    memset(sdispls, 0, size * sizeof(int));
    memset(rdispls, 0, size * sizeof(int));
    for (i = 1; i != size; i++) {
        sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
        rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
    }
    if (DEBUG) {
        /* 打印出节点发送缓冲区大小 */
        printf("Process %d send size:", rank);
        for (i = 0; i < size; i++) {
            printf(" %d\t", sendcounts[i]);
        }
        printf("\n");

        /* 打印出节点接收到的接收缓冲区大小 */
        printf("Prcess %d recv size:", rank);
        for (i = 0; i < size; i++) {
            printf(" %d\t", recvcounts[i]);
        }
        printf("\n");
    }

    /* Step-6: 每个processor将位于自己第i段的元素送到编号为i的processor */
    /* 首先交换需要与其他process交换数据的尺寸 */
    int *recvnumbers = (int*)malloc(accumulate(recvcounts, recvcounts + size, 0) * sizeof(int));
    MPI_Alltoallv(numbers, sendcounts, sdispls, MPI_INT, recvnumbers, recvcounts,
            rdispls, MPI_INT, MPI_COMM_WORLD);

    /* Step-7: 使用radix sort（基数排序）对这些bucket进行排序，即先对每个
     * bucket排序，再按顺序将各个bucket中的元素收集起来 */
    sort(recvnumbers, recvnumbers + accumulate(recvcounts, recvcounts + size, 0));
    cout << "Process " << rank << " recv:\n";
    for (i = 0; i < accumulate(recvcounts, recvcounts + size, 0); i++) {
        cout << recvnumbers[i] << " ";
    }
    cout << endl;

    if (numbers) free(numbers);
    if (recvnumbers) free(recvnumbers);
    if (sbuf) free(sbuf);
    if (rbuf) free(rbuf);
    if (sendcounts) free(sendcounts);
    if (recvcounts) free(recvcounts);
    if (sdispls) free(sdispls);
    if (rdispls) free(rdispls);
    if (allothersendcounts) free(allothersendcounts);
    MPI_Finalize();

	return 0;
}
