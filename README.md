# NDN-FC-plus
NDN-FC-plus implements what is proposed in [[1]](https://www.nz.comm.waseda.ac.jp/en/publication/#KumYosNak2020Real-World) 
[[2]](https://dl.acm.org/doi/10.1145/3417310.3431401).
It is confirmed to run on Ubuntu 18.04 or 20.04.
<br>
The program has three components: **ndn-cxx-FC**, **NFD-FC**, and **NDN-FC-skeleton**.


## ndn-cxx-FC
This ndn-cxx-FC is derived from [ndn-cxx version 0.7.1](https://named-data.net/doc/ndn-cxx/current/).  
As the original ndn-cxx, ndn-cxx-FC requires to install the following packages to build:
```
sudo apt install g++ pkg-config python3-minimal libboost-all-dev libssl-dev libsqlite3-dev
```
After installing the packages, change to the directory of ndn-cxx-FC, and enter the followings to build and install the program:
```
./waf configure
./waf
sudo ./waf install
sudo ldconfig
```


## NFD-FC
NFD-FC is derived from [NFD version 0.7.1](https://named-data.net/doc/NFD/current/).
As the original NFD, it requires to install the following packages:
```
sudo apt install libpcap-dev libsystemd-dev
```
To build and install NFD-FC, apply the following commands:
```
./waf configure
./waf
sudo ./waf install
sudo ldconfig
```


## NDN-FC-skeleton
This directory contains skeleton applications to use NDN-FC-plus.
- *sfc_consumer* acts as the consumer of the NDN-FC-plus communications and requests contents to producers.
- *sfc_producer* is the producer to provide contents.
- *sfc_function* provides the mechanism to execute functions running as Docker containers.


### sfc_consumer
sfc_consumer takes two arguments: URI style *content-name* and URI style *function-names*.
Content-name is the typical NDN content name.
On the other hand, function-names is a sequence of function names concatenated with `"/"`.  
The function named at the beginning of function-names is applyed first and the function
named at the end of function-names is applyed last.


### sfc_producer
sfc_producer takes two arguments: *content-prefix* and *content-file-name*.
Content-prefix forms the first part of the content-name requested by sfc_consumer.
Content-file-name specifies the file which holds the content to be served by the producer.
The content-prefix concatanated with content-file-name with delimiter `"/"` makes the content-name specified by sfc_consumer.


### sfc_function
sfc_function requirs one argument: *function-name*.
Function-name specifies the name of the function provided by this sfc_function.  At this moment, one sfc_function can serve only one function.


To build NDN-FC-skeleton programs, run the follows in NDN-FC-skeleton directory:
```
./waf configure
./waf
```


Executable programs can be found in its build directory. 


To run sfc_producer, a content file to be offered by the producer must be placed in the *build* directory.


To run sfc_function, a proper docker container to provide the capability of the function, must be running in the computer running sfc_function.
The python program *clientyolo.py*, need to be placed in the build directory.  Clientyolo.py must be edited to point to the right port number
served by the docker container.
Also, due to Docker bug, 
```
sudo iptables -A DOCKER -j RETURN
```
may need to be applyed in the computer.

Before running the programs in NDN-FC-skeleton, NFD-FC program must be running by `nfd-start` commnand 
on each of the computers to run sfc_producer, sfc_consumer, and sfc_function.  Also, connections 
among NFD-FC must be properly configured, e.g., *creating faces*, *setting routing information*, etc. by means of *nfdc* commnads.
