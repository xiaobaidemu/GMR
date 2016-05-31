#include <stdio.h>
#include <stdlib.h>
#include  <omp.h>  

void quickSort(int *num,int low,int high);//进行分区  
int Partition(int *num,int low,int high);//返回分离点  
int main(int argc, char* argv[])  
{  
    int num[]={2,3,5,623,32,4324,3,24};//8  
    quickSort(num,0,7);  
    int i;  
    for(i=0;i<8;i++)  
        printf("%d\n",num[i]);  
    return 0;  
}  
  
  
void quickSort(int *num,int low,int high)  
{  
    if(low<high)  
    {  
        int split=Partition(num,low,high);  
#pragma omp parallel sections//并行区域  
        {  
#pragma omp section//负责这个区域的线程对前面部分进行排序  
        quickSort(num,low,split-1);  
#pragma omp section//负责这个区域的线程对后面部分进行排序  
        quickSort(num,split+1,high);  
        }  
  
    }  
  
}  
  
int Partition(int *num,int low,int high)  
{  
    int temp=num[low];//作为中轴  
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
