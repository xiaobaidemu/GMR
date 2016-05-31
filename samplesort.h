/*************************************************************************/
/*! 此文件主要用来定义用于全局排序的函数:
 * samplesort()
*/
/**************************************************************************/

template<typename T>
void samplesort(T *tlist, int rank, int size) {
    /* Step-1: 为每个processor分配n/p个元素 */
    /* Step-2: 每个processor对自己分配的元素排序 */
    //sort(tlist, tlist + tlist_num);

    /* Step-3: 每个processor从自己分配的元素中选出p-1个分隔元素,
     * 将已排序元素分隔为平等的p段 */
    // -->T tlist[p]


    /* Step-4: 将每个processor选出的分隔元素（共有p(p-1)个）合并，
     * 再从中选出p-1个分隔元素，将这p(p-1)个元素平分为p段 */
    //MPI_Gather()


    /* Step-5: 以上一步选出的p-1个元素为界限，每个processor分配给自己的元素划分为p段 */


    /* Step-6: 每个processor将位于自己第i段的元素送到编号为i的processor */
    /* 首先交换需要与其他process交换数据的尺寸 */


    /* Step-7: 使用radix sort（基数排序）对这些bucket进行排序，即先对每个
     * bucket排序，再按顺序将各个bucket中的元素收集起来 */
}

