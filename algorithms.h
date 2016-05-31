/****************************************************
 * 该类用实求解SSSP(单源最短路径)算法
 ***************************************************/
class SSSP : public GMR {
public:
    SSSP(ssize_t startv) : startv(startv) { }
    /* 根据SSSP算法特点和需要,对图进行初始化 */
    void initGraph(graph_t *graph) {
        printf("==========>SSSP<==========\n");
        for (int i=0; i<graph->nvtxs; i++) {
            if (graph->ivsizes[i] == startv) {
                graph->fvwgts[i] = 0;
                //graph->status[i] = active;
            }
            else 
                graph->fvwgts[i] = FLT_MAX;
            for(int j = graph->xadj[i]; j < graph->xadj[i + 1]; j++)
                graph->fadjwgt[j] = 1.0f;
            graph->status[i] = active;
        }
    }

    void map(Vertex &v, std::vector<KV> &kvs) {
        /* 当第一次迭代的时候对图进行出发顶点和其他个顶点的值进行初始化 */
        kvs.push_back({v.id, v.value});
        if(DEBUG) printf("%dth map result: %d %f\n", iterNum,
                v.id, v.value);
        for (int i = 0; i < v.prenvtxs; i++) {
            kvs.push_back({v.id, v.prefwgt[i] + v.prefewgt[i]});
            if(DEBUG) printf("%dth map result: %d-->%d %f + %f\n", iterNum,
                    v.previd[i], v.id, v.prefwgt[i], v.prefewgt[i]);
        }
//         for (int i = 0; i < v.neighborSize; i++) {
//             kvs.push_back({v.neighbors[i], v.value + v.edgewgt[i]});
//             if(DEBUG) printf("%dth map result: %d-->%d %f + %f\n", iterNum,
//                     v.id, v.neighbors[i], v.value, v.edgewgt[i]);
//         }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        // dist1 [u] = Edge[v][u]
        // dist k [u] = min{ dist k-1 [u], 
        // min{ dist k-1 [j] + Edge[j][u] } }, j=0,1,…,n-1,j≠u
        float shortestPath = FLT_MAX;
        for (auto kv : kvs) {
            if (kv.value < shortestPath)
                shortestPath = kv.value;
        }
        if (DEBUG) printf("%dth reduce result: %d %f\n", iterNum,
                kvs.front().key, shortestPath);
        return {kvs.front().key, shortestPath};
    }

    /* 单源最短路经的起始节点 */
    ssize_t startv;

    /* 输出算法的执行结果 */
    virtual void printResult(graph_t *graph) {
        for (int i = 0; i < graph->nvtxs; i++)
            if (graph->nvtxs < 100 || graph->ivsizes[i] % 10000 == 0)
            //if (graph->fvwgts[i] < FLT_MAX)
            printf("path_len(%zd, %d):%f\n", startv, graph->ivsizes[i], graph->fvwgts[i]);
    }
};

/****************************************************
 * 该类用于实现PageRank算法
 * *************************************************/
class PageRank : public GMR {
public:
    /* 根据PageRank算法特点和需要,对图进行初始化 */
    void initGraph(graph_t *graph) {
        printf("==========>PageRank<==========\n");
        for (int i=0; i<graph->nvtxs; i++) {
            graph->fvwgts[i] = 1.0f;
            graph->status[i] = active;
        }
    }

    /* Map/Reduce编程模型中的Map函数 */
    void map(Vertex &v, std::vector<KV> &kvs) {
        if (v.neighborSize == 0)
            return;
        float value = 0.0;
        for (int i = 0; i < v.prenvtxs; i++) {
            value += (v.prefwgt[i] /  v.prevertexnnbor[i]);
        }
        kvs.push_back({v.id, value});
        if(DEBUG) printf("%dth map result: %d-->%f\n", iterNum, v.id, value);
//         float value = v.value / v.neighborSize;
//         for (int i = 0; i < v.neighborSize; i++) {
//             kvs.push_back({v.neighbors[i], value});
//             if(DEBUG) printf("%d %f\n", v.neighbors[i], value);
//         }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        float sum = 0.0;
        for (auto kv : kvs) {
            sum += kv.value;
        }
        /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
        sum = 0.5 * sum + (1 - 0.5) / ntxs; 
        if (DEBUG) printf("reduce result: %d %f\n", kvs.front().key, sum);
        return {kvs.front().key, sum};
    }

    /* 输出算法的执行结果 */
    virtual void printResult(graph_t *graph) {
        for (int i = 0; i < graph->nvtxs; i++)
            if (graph->nvtxs < 100 || graph->ivsizes[i] % 10000 == 0)
            //if (graph->fvwgts[i] < FLT_MAX)
            printf("pr(%zd, %f)\n", graph->ivsizes[i], graph->fvwgts[i]);
    }
};

/****************************************************
 * 该类用实求解triangleCount(三角形关系计算)算法
 ***************************************************/
class TriangleCount : public GMR {
    /* 根据PageRank算法特点和需要,对图进行初始化 */
    void initGraph(graph_t *graph) {
        printf("==========>TriangleCount<==========\n");
        /* TriangleCount算法中, 只需要迭代一次即可结束 */
        algoIterNum = 2;
        /* 子图的更新方式为累加形式, 即△ (1, 2, 3)和△ (1, 4, 5)都作为
         * 顶点1的值累加上去 */
        upmode = accu;
        /* 将所有顶点的值都赋为0, 最后存储以此顶点出发的三角形个数 */
        for (int i=0; i<graph->nvtxs; i++)
            graph->fvwgts[i] = 0;
    }

    /* Map/Reduce编程模型中的Map函数 */
    void map(Vertex &v, std::vector<KV> &kvs) {
        KV tempKV;
        for (int i = 0; i < v.neighborSize; i++) {
            tempKV.svalue.push_back(v.neighbors[i]);
        }
        for (int i = 0; i < v.neighborSize; i++) {
            std::pair<int, int> key;
            /* (a, b)与(b, a)实际上是一个键值,所以统一存为: (a, b) st. a < b */
            if (v.id < v.neighbors[i])
                tempKV.key = v.id, tempKV.skey = v.neighbors[i];
            else
                tempKV.key = v.neighbors[i], tempKV.skey = v.id;
            kvs.push_back(tempKV);
            if(DEBUG) printf("Pair.key(%d %d)->", tempKV.key, tempKV.skey);
        }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        int sum = 0;
        /* 对kvs中的value(std::list<int>)求交集 */
        if (kvs.size() == 2) {
            /* 遍历pkv1和pkv2的邻居, 寻找其相同的邻居并计数 */
            for (int i : kvs.front().svalue) {
                for (int j : kvs.back().svalue) {
                    /*只计数满足a < b < c的三角形△ (a, b, c)*/
                    if (i == j && i > kvs.front().skey) {
                        sum++;
                    }
                }
            }
        }
        if (DEBUG) printf("reduce result: (%d %d):%d\n", kvs.front().key, 
                kvs.front().skey, sum);
        return {kvs.front().key, kvs.front().skey, static_cast<float>(sum)};
    }

    /* 比较Key/Value的key */
    virtual int keyComp(KV &kv1, KV &kv2) {
        if (kv1.key == kv2.key) {
            if (kv1.skey == kv2.skey)
                return 0;
            else if (kv1.skey < kv2.skey)
                return -1;
            else
                return 1;
        }
        if (kv1.key < kv2.key)
            return -1;
        else
            return 1;
    }

    /* 输出算法的执行结果 */
    virtual void printResult(graph_t *graph) {
        float triangleSum = 0;
        for (int i=0; i<graph->nvtxs; i++)
            triangleSum += graph->fvwgts[i];
        printf("共计三角形个数:%f\n", triangleSum);
    }
};

/****************************************************
 * 该类用于求解矩阵乘法算法
 ***************************************************/
class MatrixMultiply : public GMR { };

/****************************************************
 * 该类用实求解BFS(广度优先搜索算法)算法
 ***************************************************/
class BFS : public GMR { };

/****************************************************
 * 该类用实求解connectedComponents(连图子图)算法
 ***************************************************/
class connectedComponents : public GMR { };

/****************************************************
 * 该类用实求解prime算法和kruskal算法(最小树生成)算法
 ***************************************************/
class Prime: public GMR { };

/****************************************************
 * 其他图算法, 参考:
 * The following is a quick summary of the functionality defined in both Graph and GraphOps
 * https://spark.apache.org/docs/latest/graphx-programming-guide.html#graph-operators
 ***************************************************/
class stronglyConnectedComponents : public GMR { };

/****************************************************
 * 该类用实求解KMeans机器学习算法算法
 ***************************************************/
class Kmeans : public GMR {
public:
    /* 分类的数目 */
    size_t k;

    Kmeans(size_t k) { this->k = k; }

    /* 根据Kmeans算法特点和需要,对图进行初始化 */
    void initGraph(graph_t *graph) {
        printf("==========>K-means<==========\n");
    }

    /* Map/Reduce编程模型中的Map函数 */
    void map(Vertex &v, std::vector<KV> &kvs) {
        float value = v.value / v.neighborSize;
        for (int i = 0; i < v.neighborSize; i++) {
            kvs.push_back({v.neighbors[i], value});
            if(DEBUG) printf("%d %f\n", v.neighbors[i], value);
        }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        float sum = 0.0;
        for (auto kv : kvs) {
            sum += kv.value;
        }
        /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
        sum = 0.5 * sum + (1 - 0.5) / ntxs; 
        if (DEBUG) printf("reduce result: %d %f\n", kvs.front().key, sum);
        return {kvs.front().key, sum};
    }

    /* 输出算法的执行结果 */
    virtual void printResult(graph_t *graph) { }
};
