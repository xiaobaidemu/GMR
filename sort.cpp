#include <iostream>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include  <omp.h>  

struct KV {
    int key; float value;
    /* skey, svalue作为结构体KV中辅助存储键值的单元 */
    KV() {}
    KV(int key, float value) : key(key), value(value){}
    bool operator<=(KV &kv) {
        return key <= kv.key;
    }
    bool operator>=(KV &kv) {
        return key >= kv.key;
    }
};
bool comp(const KV &k1,const KV &k2)
{
    return k1.key < k2.key;
}


template <typename T>
void quickSort(std::vector<T> &num,int low,int high)  
{  
    if(low<high)  
    {  
        int split=Partition(num,low,high);  
//#pragma omp parallel sections//并行区域  
        {  
//#pragma omp section//负责这个区域的线程对前面部分进行排序  
        quickSort(num,low,split-1);  
//#pragma omp section//负责这个区域的线程对后面部分进行排序  
        quickSort(num,split+1,high);  
        }  
    }  
}  
  
template<typename T>
int Partition(std::vector<T> &num,int low,int high)  
{  
    T temp=num[low];//作为中轴  
    while(low<high)  
    {  
        while(low<high&&num[high]>=temp)high--;  
        num[low]=num[high];  
        while(low<high&&num[low]<=temp)low++;  
        num[high]=num[low];  
    }  
    num[low]=temp;  
    return low;//返回中轴的位置，再进行分离  
}

int main(int argc, char* argv[])  
{  
    std::vector<KV> kvs;
    std::vector<KV> kvs_q;
    /*kvs.push_back({12, 34.12});
    kvs.push_back({34, 45.2});
    kvs.push_back({1, 4});
    kvs.push_back({68, 3.2});
    kvs.push_back({3, 4.5});
    kvs.push_back({3, 3.3});
    kvs.push_back({4, 5.5});
    kvs.push_back({6, 5.6});
    kvs.push_back({23, 89});*/
    int key;
    srand((unsigned)time(NULL));
    for (int i = 0; i < 100000000; i++) {
        key = rand() % 100000000;
        kvs.push_back({key, 89.99});
        kvs_q.push_back({key, 89.99});
    }

    time_t start = time(NULL);
    //quickSort(kvs, 0, kvs.size() - 1);
   // sort(kvs.begin(), kvs.end(), [](KV kv1, KV kv2) {return kv1.key < kv2.key;});
    sort(kvs.begin(),kvs.end(),comp);
    //for (auto e : kvs)
     //   std::cout << e.key << " " << e.value << std::endl;
    std::cout << "time consumption for stlsort:" << difftime(time(NULL), start) << " secs.\n";
    start = time(NULL);
    quickSort(kvs_q, 0, kvs.size()-1);
    std::cout << "time consumption for quicksort:" << difftime(time(NULL), start) << " secs.\n";
    return 0;  
}  
  
