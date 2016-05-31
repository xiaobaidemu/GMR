#GraphMapReduce: 基于MapReduce编程模型的图计算框架
[![Build Status](https://travis-ci.org/flowlo/pcp-vns.png?branch=master)](https://travis-ci.org/flowlo/pcp-vns)    
(名词约束: 顶点Vertex-图中顶点;节点Process-计算单元节点),目录说明:     


> 代码主要包含四个文件: gmr.cpp gmr.h algorithms.h graph.h     
> |__graph/---------#此目录包含测试用的图例数据     
> |__include/-------#此目录包含所使用到的第三方库的头文件(目前只用到了ParMetis，去掉了GKlib)     
> |__lib/------------#包含了使用到的第三方库     
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
| 举例4 |./startgmr.sh cluster hosts sssp metis 4elt                             |  |
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

2. metis输出的子图格式
为了将全图的不同部分放到不同的计算节点进行并行计算，需要将原图划分为若干子图。划分工具采用开源的Parmetis进行(为方便使用，正在进行整合)。Parmetis是基于MPI进行大规模的子图划分，为了方便和适应我们的算法，我们对Parmetis的输出结果进行了重写，每个输出的节点的格式如下:
```
#节点id    节点权重       邻居1的id  邻居1所在进程        邻居1所在边权重 ...邻居N的id  邻居N所在进程        邻居N所在边权重
vertex_id vertex_weight neighbor1 neighbor1.location edge1.weight ... neighborN neighborN.location edgeN.weight
```
为方便测试，测试数据目录graph/目录中已经分好了三个不同规模的图small、4elt、mdual，定点数和边数从几十个到几百万个。

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

#### 4.2.3 PageRank终止点问题和陷阱问题
上述上网者的行为是一个马尔科夫过程的实例，要满足收敛性，需要具备一个条件：
图是强连通的，即从任意网页可以到达其他任意网页：
互联网上的网页不满足强连通的特性，因为有一些网页不指向任何网页，如果按照上面的计算，上网者到达这样的网页后便走投无路、四顾茫然，导致前面累 计得到的转移概率被清零，这样下去，最终的得到的概率分布向量所有元素几乎都为0。假设我们把上面图中C到A的链接丢掉，C变成了一个终止点，得到下面这个图：

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/214258_5e3a6ed7_496314.jpeg "在这里输入图片标题")

另外一个问题就是陷阱问题，即有些网页不存在指向其他网页的链接，但存在指向自己的链接。比如下面这个图：

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/214318_aadc9dd1_496314.jpeg "在这里输入图片标题")

上网者跑到C网页后，就像跳进了陷阱，陷入了漩涡，再也不能从C中出来，将最终导致概率分布值全部转移到C上来，这使得其他网页的概率分布值为0，从而整个网页排名就失去了意义。

### 4.2 单源最短路算法SSSP（DJ算法）

### 4.3 TriangleCount

### 4.4 并行广度优先搜索算法的MapReduce实现

### 4.5 二度人脉算法:广度搜索算法

## 五、对比实验
|Processor\Platform |   GMR      |    Spark      |   GraphX       |    GraphLab      |     Pregel  |   
|-------------------|------------|---------------|----------------|------------------|-------------|
|      1            |            |               |                |                  |             |   
|      3            |            |               |                |                  |             |   
|      8            |            |               |                |                  |             |   
|      16           |            |               |                |                  |             |   
|      32           |            |               |                |                  |             |   