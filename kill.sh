kill -9 `ps -e|grep mpirun|awk '{print $1}'`
