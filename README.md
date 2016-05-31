#GraphMapReduce: 基于MapReduce和MPI的图计算框架（Master分支已经不再使用，转至without_partition_tool分支）
请见without_partition_tool分支README.md
[![Build Status](https://travis-ci.org/flowlo/pcp-vns.png?branch=master)](https://travis-ci.org/flowlo/pcp-vns)    
(名词约束: 顶点Vertex-图中顶点;节点Process-计算单元节点),目录说明:     


> 代码主要包含四个文件: gmr.cpp gmr.h algorithms.h graph.h     
> |__graph/---------#此目录包含测试用的图例数据     
> |__gmr.cpp------#程序的main函数入口和迭代循环     
> |__gmr.h---------#包含主要的计算过程函数computing()和计算结果更新函数updateGraph()     
> |__algorithm.h---#常用图算法的MapReduce实现     
> |__graph.h-------#定义了图数据结果和常用的集中图操作函数   
 

## 一、 编译和运行
### 1. 编译gmr
make clean && make    
### 2. 运行gmr
|          单机运行      |                     |
| -------------------------------------------------------------------------|-----------|
| 命令  |./startgmr.sh [algorithm] [partition] [graphfile]                                                | |
| 支持  |./startgmr.sh [pagerank 或 sssp 或 trianglecount] [random 或 metis] [small 或 4elt 或 mdual]       | |
| 举例1 |./startgmr.sh                                             |  | 
| 举例2 |./startgmr.sh pagerank                                    |  |
| 举例3 |./startgmr.sh sssp random                                 |  |
| 举例4 |./startgmr.sh sssp metis 4elt                             |  |
| 举例5 |./startgmr.sh pagerank metis small                        |  |
| 或者直接运行 |mpirun -np 3 gmr pagerank; ii.) mpirun -np 3 sssp random; iii.)mpirun -np 3 trianglecount metis 4elt. ||


|          集群运行      |                     |
| -------------------------------------------------------------------------|-----------|
| 命令  |./startgmr.sh cluster hosts [algorithm] [partition] [graphfile]                                  | |
| 支持  |./startgmr.sh [pagerank 或 sssp 或 trianglecount] [random 或 metis] [small 或 4elt 或 mdual]       | |
| 举例1 |./startgmr.sh cluster hosts                                             |  | 
| 举例2 |./startgmr.sh cluster hosts pagerank                                    |  |
| 举例3 |./startgmr.sh cluster hosts sssp random                                 |  |
| 举例5 |./startgmr.sh cluster hosts pagerank metis small                        |  |
| 或者直接运行 |i.) mpirun -machinefile hosts -np 10 gmr; ii.) mpirun -machinefile hosts -np 10 gmr pagerank random; iii.)mpirun -machinefile hosts -np 10 gmr pagerank trianglecount metis 4elt    ||
   
> 注: i.)如果使用metis分图方式, 需要先使用metis分图工具将图文件分图, gpmetis工具位于目录pathtogmr/include/metis/,根据平台不同可能需要编译;ii.)使用random分图格式的图文件格式，文件每行记录from_vid to_vid.        

### 3. (non-mandatory)切图
目前提供了两种分图方式:
1. 随机切分方式    
MPI进程按照其进程号依次等分的读取图文件的顶点，切分文件的时候，一个顶点只分配给一个MPI进程。(调用gmr启动脚本startgmr.sh默认采用这种分图方式。)    
2. Metis分图方式
为了最大的保留图内顶点的链接信息，不仅可以减少MPI进程之间的传输量，还能最大化保持图内顶点生成的键值对的局部性，从而减少Map、Reduce、Sort的工作量。所以，GMR另外还提供metis工具进行切图(需要重新编译metis代码，然后运行"gpmetis graphfilename partsnumber"), 或者直接采用切好的示例图库(graph/)中的图进行测试(small.subgraph.* 4elt.graph.* mdual.graph.*分别为不同规模的图例).
目前切图工具采用了metis库，其源码和说明位于include/metis中，其编译使用可参考include/metis/README.md。


## 二. 框架的基础
#### 1. MPI:
结算进程之间通信通过MPI实现；
#### 2. MapReduce编程模型
#### 3. 图划分:
目前采用两种方式的输入图格式:
1. 普通图文件格式: from_vid to_vid
这种输入图格式，在运行程序的时候需要选择"random"的partition方式(分图方式)。程序的各个进程将会并行且均分的读取文件的相应部分。(这种方式会导致迭代计算过程中信息交换量急剧增加.)

注意：metis输出的子图格式(不再使用任何分图工具，只使用随机分图方法)
之前有使用过metis ,zoltan等分图工具，但是分图效果很差，时间开销太大且往往导致负载不均衡

## 三、迭代计算过程
#### 1. 数据交换:
第一步,先遍历自己计算的子图graph与其他子图的邻居情况,并收集需要向其他节点发送的字节数,并申请发送缓冲区;

第二步,通过MPI_Alltoall()与其他节点交换其他节点需要接受的字节数,每个节点收到信息后,各自计算和申请接受数据需要的空间。

第三步,再次遍历自己计算的子图graph,并将需要发往其他节点的顶点信心拷贝到发送缓存char *sb;

第四部,调用MPI_Alltoallv(),将发送缓存中的数据发往各节点.
#### 2. 计算1th/2:map
将子图graph和接受缓冲区中的数据实例化为顶点Vertex，再调用业务逻辑函数map将Vertex生成key/value list。
#### 3. 对生成key/value list进行排序: sort
#### 4. 计算2th/2:reduce
将排序好的key/value list按照业务逻辑函数reduce进行规约.
#### 5. 将reduce计算的结果更新到graph中    
#### 6. (non-mandatory)为兼容非图结构的MapReduce计算, 框架(将)在Map与Reduce之间实现除局部排序意外的全局排序。    
图结构的MapReduce计算和非图结构的MapReduce计算在计算步骤上并不一样，其异同如下图所示，框架为了同时支持非图结构数据的MapReduce计算，在Map、Reduce之间同时(将)实现了全局排序。    
![输入图片说明](http://git.oschina.net/uploads/images/2016/0218/123450_97c46b95_496314.png "在这里输入图片标题")

## 四. 例子
### 4.1 PageRank
#### 4.1.1. 如下包含10个顶点的简单图，划分之后包含三个子图subgraphs[3]:
![输入图片说明](http://git.oschina.net/uploads/images/2016/0120/132332_24897e71_496314.png "在这里输入图片标题")

#### 4.1.2. 迭代过程

- 每个子图现将自己的边界顶点发送给其所连接的邻居节点，采用MPI_Alltoall()实现；
- 在每个计算节点的内部，将每个顶点<id, loc, [neighbors]执行map函数, value>映射为若干键值对:
          > {key, value1},其中key in [neighbors], value1 = value / neighbors.size()
```c++
void map(Vertex &v, std::list<KV> &kvs){
    int neighbor_count = 0;
    while(v.neighbors[neighbor_count] != 0)neighbor_count++;

    float value = v.value / neighbor_count;
    for (int i = 0; i < neighbor_count; i++)
        kvs.push_back({v.neighbors[i], value});
}
```
- 在每个节点内将map生成的键值对按键值进行排序
- 根据键值，对键值相同的键值组执行reduce函数
```c++
KV reduce(std::list<KV> &kvs) {
   float sum = 0.0;
    for (auto kv : kvs) {
        sum += kv.value;
    }

    /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
    sum = 0.5 * sum + (1 - 0.5) / (sizeof(vs) / sizeof(Vertex) - 1); 
    return {kvs.front().key, sum};
}
```

