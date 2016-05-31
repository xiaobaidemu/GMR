#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* List terminator for GKfree() */
#define LTERM                   (void **) 0
#define GK_GRAPH_FMT_METIS      1
#define GRAPH_DEBUG             true 
#define GRAPH_INFO              true 

/* ntxs:          当前进程使用的子图中顶点数
 * nedges:        当前进程使用的子图边条数
 * NODE_STATUS:   顶点状态 */
int ntxs = 0;
int nedges = 0;
enum NODE_STATUS {active, inactive};

static int numbering = 0;
typedef struct graph_t {
  int nvtxs; /* total vertices in in my partition */
  int nedges;   /* total number of neighbors of my vertices */
  int *ivsizes;    /* global ID of each of my vertices */
  int *xadj;    /* xadj[i] is location of start of neighbors for vertex i */
  int *adjncy;      /* adjncys[xadj[i]] is first neighbor of vertex i */
  int *adjloc;     /* process owning each nbor in adjncy */

  int32_t *ivwgts;              /*!< The integer vertex weights */
  int32_t *iadjwgt;             /*!< The integer edge weights */

  /* add for new functions */
  int prenedges;
  int *prexadj;
  int *preadjncy;
  float *prefvwgts;
  float *prefadjwgt;
  int *prestatus;

  float *fvwgts;
  float *fadjwgt;
  int *status;
} graph_t;

struct Edge {
    int vid;
    int fvid;
    float fwgt;
    float fewgt;
};

unsigned int simple_hash(unsigned int *key, unsigned int n)
{
  unsigned int h, rest, *p, bytes, num_bytes;
  char *byteptr;

  num_bytes = (unsigned int) sizeof(int);

  /* First hash the int-sized portions of the key */
  h = 0;
  for (p = (unsigned int *)key, bytes=num_bytes;
       bytes >= (unsigned int) sizeof(int);
       bytes-=sizeof(int), p++){
    h = (h*2654435761U) ^ (*p);
  }

  /* Then take care of the remaining bytes, if any */
  rest = 0;
  for (byteptr = (char *)p; bytes > 0; bytes--, byteptr++){
    rest = (rest<<8) | (*byteptr);
  }

  /* Merge the two parts */
  if (rest)
    h = (h*2654435761U) ^ rest;

  /* Return h mod n */
  return (h%n);
}

/* Function to find next line of information in input file */
static int get_next_line(FILE *fp, char *buf, int bufsize)
{
  int i, cval, len;
  char *c;

  while (1){

    c = fgets(buf, bufsize, fp);

    if (c == NULL)
      return 0;  /* end of file */

    len = strlen(c);

    for (i=0, c=buf; i < len; i++, c++){
      cval = (int)*c; 
      if (isspace(cval) == 0) break;
    }
    if (i == len) continue;   /* blank line */
    if (*c == '#') continue;  /* comment */

    if (c != buf){
      strcpy(buf, c);
    }
    break;
  }

  return strlen(buf);  /* number of characters */
}

/* Function to find next line of information in input file */
static int get_next_vertex(FILE *fp, int *val)
{
  char buf[512];
  char *c = buf;
  int num, cval, i = 0, j = 0, len = 0, bufsize = 512;
  while (1){
    c = fgets(buf, bufsize, fp);
    /* end of the file */
    if (c == NULL) {
        if (j > 2)
            val[1] = j - 2;
        return j;
    }
    
    len = strlen(c);
    for (i=0, c=buf; i < len; i++, c++){
      cval = (int)*c; 
      if (isspace(cval) == 0) break;
    }
    if (i == len) continue;   /* blank line */
    if (*c == '#') continue;  /* comment */
    
    int from, to;
    num = sscanf(buf, "%d%*[^0-9]%d", &from, &to);
    if (num != 2)
        perror("输入文件格式错误.\n");
    if (j > 0) {
        if (from + numbering == val[0])
            val[j++] = to + numbering;
        else {
            fseek(fp, -1 * len, SEEK_CUR);
            break;
        }
    }
    else {
        if (from == 0) numbering = 1;
        val[j++] = from + numbering, val[j++] = 0, val[j++] = to + numbering;
    }
  }
  val[1] = j - 2;
  return j;
}

/* Proc 0 notifies others of error and exits */

static void input_file_error(int numProcs, int tag, int startProc)
{
int i, val[2];

  val[0] = -1;   /* error flag */

  fprintf(stderr,"ERROR in input file.\n");

  for (i=startProc; i < numProcs; i++){
    /* these procs have posted a receive for "tag" expecting counts */
    MPI_Send(val, 2, MPI_INT, i, tag, MPI_COMM_WORLD);
  }
  for (i=1; i < startProc; i++){
    /* these procs are done and waiting for ok-to-go */
    MPI_Send(val, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
  }

  MPI_Finalize();
  exit(1);
}


/*
 * Read the graph in the input file and distribute the vertices.
 */

void read_input_file(int myRank, int numProcs, char *fname, graph_t *graph)
{
  char buf[512];
  int bufsize;
  int numGlobalVertices, numGlobalNeighbors;
  int num, nnbors, ack=0;
  int vGID;
  int i, j, procID;
  int vals[102400], send_count[3];
  int *idx;
  unsigned int id;
  FILE *fp;
  MPI_Status status;
  int ack_tag = 5, count_tag = 10, id_tag = 15;
  graph_t *send_graph;
  double readfile_start = MPI_Wtime();

  if (myRank == 0){

    bufsize = 512;

    fp = fopen(fname, "r");

    /* Get the number of vertices */

    num = get_next_line(fp, buf, bufsize);
    if (num == 0) input_file_error(numProcs, count_tag, 1);
    num = sscanf(buf, "%d", &numGlobalVertices);
    if (num != 1) input_file_error(numProcs, count_tag, 1);

    /* Get the number of vertex neighbors  */

    num = get_next_line(fp, buf, bufsize);
    if (num == 0) input_file_error(numProcs, count_tag, 1);
    num = sscanf(buf, "%d", &numGlobalNeighbors);
    if (num != 1) input_file_error(numProcs, count_tag, 1);

    /* Allocate arrays to read in entire graph */
    graph->xadj = (int *)calloc(sizeof(int), numGlobalVertices + 1);
    graph->adjncy = (int *)calloc(sizeof(int), numGlobalNeighbors);
    graph->adjloc = (int *)calloc(sizeof(int), numGlobalNeighbors);
    graph->ivsizes = (int *)calloc(sizeof(int), numGlobalVertices);

    int edges = 0;
    graph->xadj[0] = 0;
    int old_vid = 1;
    for (i=0; i < numGlobalVertices && edges < numGlobalNeighbors; i++){
      num = get_next_vertex(fp, vals);
//       printf("get num:%d\n", num);
      if (num == 0) {
        for (j = 1; i < numGlobalVertices; j++, i++) {
            vGID = vals[0] + j;
            nnbors = 0;
            graph->ivsizes[i] = (int)vGID;
            graph->xadj[i+1] = graph->xadj[i] + nnbors;
        }
        break;
      }
      if (num < 2) input_file_error(numProcs, count_tag, 1);
      /* 如果读取的图顶点的id, 与前一个读取的图顶点id不连续, 则需要补足中间不连续部分 */
      if (vals[0] - old_vid > 1) {
        if (vals[0] > numGlobalVertices) {
            perror("读取的图顶点id大于了图的最大顶点数.\n");
            MPI_Finalize();
            exit(1);
        }
        for (j = vals[0] - old_vid - 1; j > 0; j--, i++) {
            vGID = vals[0] - j;
            nnbors = 0;
            graph->ivsizes[i] = (int)vGID;
            graph->xadj[i+1] = graph->xadj[i] + nnbors;
        }
      }

      old_vid = vGID = vals[0]; // vGID = vals[0];
      nnbors = vals[1];
      edges += nnbors;

      if (num < (nnbors + 2)) input_file_error(numProcs, count_tag, 1);
      graph->ivsizes[i] = (int)vGID;

      for (j=0; j < nnbors; j++){
        graph->adjncy[graph->xadj[i] + j] = (int)vals[2 + j];
      }

      graph->xadj[i+1] = graph->xadj[i] + nnbors;
    }
    if (edges != numGlobalNeighbors)
        input_file_error(numProcs, count_tag, 1);
    printf("成功读取文件, 耗时:%f\n", MPI_Wtime() - readfile_start);
    fclose(fp);

    /* Assign each vertex to a process using a hash function */
    for (i=0; i < numGlobalNeighbors; i++){
      id = (unsigned int)graph->adjncy[i];
      graph->adjloc[i] = simple_hash(&id, numProcs);
    } 

    /* Create a sub graph for each process */
    send_graph = (graph_t *)calloc(sizeof(graph_t) , numProcs);

    for (i=0; i < numGlobalVertices; i++){
      id = (unsigned int)graph->ivsizes[i];
      procID = simple_hash(&id, numProcs);
      send_graph[procID].nvtxs++;
      /////////////////////////////////////////////////////
      for (j = graph->xadj[i]; j < graph->xadj[i + 1]; j++) {
        unsigned int adjncy = graph->adjncy[j];
        int procIDofadjncy = simple_hash(&adjncy, numProcs);
        send_graph[procIDofadjncy].prenedges++;
      }
      /////////////////////////////////////////////////////
    }

    for (i=0; i < numProcs; i++){
      num = send_graph[i].nvtxs;
      send_graph[i].ivsizes = (int *)calloc(sizeof(int), num);
      send_graph[i].xadj = (int *)calloc(sizeof(int) , (num + 1));
      /////////////////////////////////////////////////////
//       num = send_graph[i].prenedges;
//       send_graph[i].preadjncy = (int*)calloc(sizeof(int), num);
      /////////////////////////////////////////////////////
    }

    /* 新增注释: 用来缓存目前为止各个进程已放入的顶点数 */
    idx = (int *)calloc(sizeof(int), numProcs);
//     int *preadjoffset = (int*)calloc(sizeof(int), numProcs);

    for (i=0; i < numGlobalVertices; i++){

      id = (unsigned int)graph->ivsizes[i];
      nnbors = graph->xadj[i+1] - graph->xadj[i];
      procID = simple_hash(&id, numProcs);

      j = idx[procID];
      send_graph[procID].ivsizes[j] = (int)id;
      send_graph[procID].xadj[j+1] = send_graph[procID].xadj[j] + nnbors;

      /////////////////////////////////////////////////////
//       for (k = graph->xadj[i]; k < graph->xadj[i + 1]; k++) {
//         unsigned int adjncy = graph->adjncy[j];
//         int procIDofadjncy = simple_hash(&adjncy, numProcs);
//         //TODO: 可取去除他们, 通过第一次数据同步获取这些信息
//         send_graph[procIDofadjncy].preadjncy[preadjoffset[procIDofadjncy]++] = id;//////EE
//         send_graph[procIDofadjncy].prexadj[k];//////////////////////EE
//       }
      /////////////////////////////////////////////////////
      idx[procID] = j+1;
    }

    for (i=0; i < numProcs; i++){
      num = send_graph[i].xadj[send_graph[i].nvtxs];
      send_graph[i].adjncy = (int *)malloc(sizeof(int) * num);
      send_graph[i].adjloc= (int *)malloc(sizeof(int) * num);
      send_graph[i].nedges = num;
    }

    memset(idx, 0, sizeof(int) * numProcs);

    for (i=0; i < numGlobalVertices; i++){
      id = (unsigned int)graph->ivsizes[i];
      nnbors = graph->xadj[i+1] - graph->xadj[i];
      procID = simple_hash(&id, numProcs);
      j = idx[procID];

      if (nnbors > 0){
        memcpy(send_graph[procID].adjncy + j, graph->adjncy + graph->xadj[i],
               nnbors * sizeof(int));
  
        memcpy(send_graph[procID].adjloc + j, graph->adjloc + graph->xadj[i],
               nnbors * sizeof(int));
        idx[procID] = j + nnbors;
      }
    }

    free(idx);

    /* Process zero sub-graph */
    free(graph->ivsizes);
    free(graph->xadj);
    free(graph->adjncy);
    free(graph->adjloc);

    *graph = send_graph[0];
    graph->fvwgts  = (float *)calloc(sizeof(int), send_graph[0].nvtxs);
    graph->fadjwgt = (float *)calloc(sizeof(int), send_graph[0].nedges);
    graph->status  = (int *)calloc(sizeof(int), send_graph[0].nvtxs);

    ///////////////////////////////////////////////////////////
    graph->prexadj = (int *)calloc(sizeof(int), send_graph[0].nvtxs + 1);
    graph->preadjncy = (int *)calloc(sizeof(int), send_graph[0].prenedges);
    graph->prefvwgts = (float *)calloc(sizeof(float), send_graph[0].prenedges);
    graph->prefadjwgt = (float *)calloc(sizeof(float), send_graph[0].prenedges);
    graph->prestatus = (int *)calloc(sizeof(int), send_graph[0].nvtxs);
    ///////////////////////////////////////////////////////////
    /* Send other processes their subgraph */

    for (i=1; i < numProcs; i++){
      send_count[0] = send_graph[i].nvtxs;
      send_count[1] = send_graph[i].nedges;
      send_count[2] = send_graph[i].prenedges;

      MPI_Send(send_count, 3, MPI_INT, i, count_tag, MPI_COMM_WORLD);
      MPI_Recv(&ack, 1, MPI_INT, i, ack_tag, MPI_COMM_WORLD, &status);

      if (send_count[0] > 0){
        MPI_Send(send_graph[i].ivsizes, send_count[0], MPI_INT, i,
                id_tag, MPI_COMM_WORLD);
        free(send_graph[i].ivsizes);

        MPI_Send(send_graph[i].xadj, send_count[0] + 1, MPI_INT, i,
                id_tag + 1, MPI_COMM_WORLD);
        free(send_graph[i].xadj);

        if (send_count[1] > 0){
          MPI_Send(send_graph[i].adjncy, send_count[1], MPI_INT, i,
                  id_tag + 2, MPI_COMM_WORLD);
          free(send_graph[i].adjncy);

          MPI_Send(send_graph[i].adjloc, send_count[1], MPI_INT, i, id_tag + 3, MPI_COMM_WORLD);
          free(send_graph[i].adjloc);
        }

        ///////////////////////////////////////////////////////////
//         if (send_count[2] > 0) {
           // Send the messages: preadjncy, prefvwgts, prefadjwgt, prestatus 
//           MPI_Send(send_graph[i].prexadj, send_count[0] + 1, MPI_INT, i,
//                   id_tag + 4, MPI_COMM_WORLD);
//           free(send_graph[i].prexadj);
//           MPI_Send(send_graph[i].preadjncy, send_count[2], MPI_INT, i,
//                   id_tag + 5, MPI_COMM_WORLD);
//           free(send_graph[i].preadjncy);
//         }
        ///////////////////////////////////////////////////////////
      }
    }

    free(send_graph);
    /* signal all procs it is OK to go on */
    ack = 0;
    for (i=1; i < numProcs; i++){
      MPI_Send(&ack, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
  }
  else{
    MPI_Recv(send_count, 3, MPI_INT, 0, count_tag, MPI_COMM_WORLD, &status);

    if (send_count[0] < 0){
      MPI_Finalize();
      exit(1);
    }

    ack = 0;

    graph->nvtxs = send_count[0];
    graph->nedges  = send_count[1];
    graph->prenedges  = send_count[2];

    if (send_count[0] > 0){

      graph->ivsizes = (int *)calloc(sizeof(int), send_count[0]);
      graph->xadj = (int *)calloc(sizeof(int), (send_count[0] + 1));
      graph->fvwgts  = (float *)calloc(sizeof(int), send_count[0]);
      graph->status  = (int *)calloc(sizeof(int), send_count[0]);
      graph->prestatus  = (int *)calloc(sizeof(int), send_count[0]);
      graph->prexadj = (int *)calloc(sizeof(int), send_count[0] + 1);

      if (send_count[1] > 0){
        graph->adjncy  = (int *)calloc(sizeof(int), send_count[1]);
        graph->fadjwgt = (float *)calloc(sizeof(int), send_count[1]);
        graph->adjloc = (int *)calloc(sizeof(int), send_count[1]);
      }

      //////////////////////////////////////////////////////////////
      if (send_count[2] > 0) {
          graph->preadjncy = (int *)calloc(sizeof(int), send_count[2]);
          graph->prefvwgts = (float *)calloc(sizeof(float), send_count[2]);
          graph->prefadjwgt = (float *)calloc(sizeof(float), send_count[2]);
      }
      //////////////////////////////////////////////////////////////
    }

    MPI_Send(&ack, 1, MPI_INT, 0, ack_tag, MPI_COMM_WORLD);

    if (send_count[0] > 0){
      MPI_Recv(graph->ivsizes,send_count[0],MPI_INT, 0,
              id_tag, MPI_COMM_WORLD, &status);
      MPI_Recv(graph->xadj,send_count[0] + 1, MPI_INT, 0,
              id_tag + 1, MPI_COMM_WORLD, &status);

      if (send_count[1] > 0){
        MPI_Recv(graph->adjncy,send_count[1], MPI_INT, 0,
                id_tag + 2, MPI_COMM_WORLD, &status);
        MPI_Recv(graph->adjloc,send_count[1], MPI_INT, 0, id_tag + 3, MPI_COMM_WORLD, &status);
      }

        ///////////////////////////////////////////////////////////
//         if (send_count[2] > 0) {
//             MPI_Recv(graph->prexadj, send_count[0] + 1, MPI_INT, 0,
//                 id_tag + 4, MPI_COMM_WORLD, &status);
           // receive the messages: preadjncy, prefvwgts, prefadjwgt, prestatus 
//             MPI_Recv(graph->preadjncy,send_count[2], MPI_INT, 0,
//                 id_tag + 5, MPI_COMM_WORLD, &status);
//         }
        ///////////////////////////////////////////////////////////
    }

    /* ok to go on? */

    MPI_Recv(&ack, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    if (ack < 0){
      MPI_Finalize();
      exit(1);
    }
  }
    /* 将graph->prestatus设置为inactive, status设置为active */
  for (i = 0; i < graph->nvtxs; i++) {
    graph->prestatus[i] = inactive;
    graph->status[i] = inactive;
  }
//   printf("Graph==>%d:", myRank);
//   for (i = 0; i < graph->nvtxs; i++) {
//     printf(" %d-->(", graph->ivsizes[i]);
//     for (j = graph->xadj[i]; j < graph->xadj[i + 1]; j++) {
//         //if (graph->adjloc[j] > numProcs && myRank == 2);
//         printf(" %d@%d ", graph->adjncy[j], graph->adjloc[j]);
//     }
//     printf(")  ");
//   }
//   printf("\n");
}

/* 获取发往其他各运算节点的字节数 */
int *getSendBufferSize(const graph_t *graph, const int psize, const int rank, 
        int *sendcounts) {
    memset(sendcounts, 0, psize * sizeof(int));
    /* 先遍历一次需要发送的数据，确定需要和每个节点交换的数据 */
    for (int i=0; i<graph->nvtxs; i++) {
        /* 如果当前顶点vertex为iactive, 则不用发送 */
        if (graph->status[i] == inactive) continue;

        /* visited 用于记录当前遍历的顶点是否已经发送给节点adjloc[j] */
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            /* 如果当前顶点的终止顶点为在当前节点, 则不必发送 */
            //if (graph->adjloc[j] == rank) continue;
            sendcounts[graph->adjloc[j]]++;
        }
    }

    /* 发送数据格式为: neighbor_i, v.id, v.wgt, v.edge_i.wgt */
    return sendcounts;
}

/* 将要发送的数据从graph中拷贝到发送缓存sb中 */
Edge *getSendbuffer(graph_t *graph, int *sdispls, 
        int psize, int rank, Edge *sb) {
    /* 将要发送顶点拷贝到对应的缓存: 内存拷贝(是否有方法减少拷贝?) */
    int *offsets = (int*)calloc(sizeof(int), psize);
    for (int i = 0; i < graph->nvtxs; i++) {
        /* 如果当前顶点vertex为iactive, 则不用发送 */
        if (graph->status[i] == inactive) continue;

        Edge edge;
        for (int j = graph->xadj[i]; j< graph->xadj[i+1]; j++) {
            //if (graph->adjloc[j] == rank) continue;
            edge.vid = graph->adjncy[j];
            edge.fvid = graph->ivsizes[i];
            edge.fwgt = graph->fvwgts[i];
            edge.fewgt = graph->fadjwgt[j];
            sb[sdispls[graph->adjloc[j]] + offsets[graph->adjloc[j]]] = edge;
            offsets[graph->adjloc[j]]++;
        }
    }
    if (offsets) free(offsets);
    return sb;
}

void graph_Free(graph_t *graph) {
    if (graph->nvtxs > 0) {
        free(graph->ivsizes);
        free(graph->xadj);
    }
//     if (graph->ivsizes != NULL) free(graph->ivsizes);
//     if (graph->xadj != NULL) free(graph->xadj);
//     if (graph->adjncy != NULL) free(graph->adjncy);
//     if (graph->adjloc != NULL) free(graph->adjloc);
// 
//     if (graph->ivwgts != NULL) free(graph->ivwgts);
//     if (graph->iadjwgt != NULL) free(graph->iadjwgt);
// 
//     if (graph->fvwgts != NULL) free(graph->fvwgts);
//     if (graph->adjwgt != NULL) free(graph->adjwgt);
//     if (graph->status != NULL) free(graph->status); 
}

/* 打印图信息 */
void displayGraph(graph_t *graph) {
//     int hasvwgts, hasvsizes, hasewgts, haseloc;
//     hasewgts  = (graph->iadjwgt || graph->fadjwgt);
//     hasvwgts  = (graph->ivwgts || graph->fvwgts);
//     hasvsizes = (graph->ivsizes || graph->fvsizes);
//     haseloc   = graph->adjloc != NULL;
// 
//     for (int i=0; i<graph->nvtxs; i++) {
//         if (hasvsizes) {
//             if (graph->ivsizes)
//                 printf(" %d", graph->ivsizes[i]);
//             else
//                 printf(" %f", graph->fvsizes[i]);
//         }
//         if (hasvwgts) {
//             if (graph->ivwgts)
//                 printf(" %d", graph->ivwgts[i]);
//             else
//                 printf(" %f", graph->fvwgts[i]);
//         }
// 
//         for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
//             printf(" %d", graph->adjncy[j]);
//             if (haseloc)
//                 printf(" %d", graph->adjloc[j]);
//             if (hasewgts) {
//                 if (graph->iadjwgt)
//                     printf(" %d", graph->iadjwgt[j]);
//                 else 
//                     printf(" %f", graph->fadjwgt[j]);
//                 }
//         }
//         printf("\n");
//     }
}
