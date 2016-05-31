# GMR
一个基于MapReduce和MPI的图计算模型
==
###编译：
make
###运行：
mpirun -np <machine_num> -machinefile <machinelist_name> ./igmr <algorithm_name> ../<graph.name>
###关于数据：
可以使用stanford的http://snap.stanford.edu/data/的图数据进行研究<br>
我们使用的数据是：soc-LiveJournal1	Directed	4,847,571	68,993,773	LiveJournal online social network
###与spark的graphx进行性能对比
所用命令行<br>
bin/spark-submit --class org.apache.spark.examples.graphx.Analytics lib/graphx_test.jar sp ../soc-LiveJournal1.txt
--numEPart=3 单机版<br>

time -p bin/spark-submit --class org.apache.spark.examples.graphx.Analytics --master spark://A281:7077 lib/graphx_0.jar sp ../soc-LiveJournal1.txt --numEPart=3<br>


