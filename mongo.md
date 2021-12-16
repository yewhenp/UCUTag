## MongoDBCXX installation
- download, compile and install [mongo c driver](https://github.com/mongodb/mongo-c-driver)
- download, compile and install [mongo cxx driver](http://mongocxx.org/mongocxx-v3/installation/linux/):
    ```
  curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.6.6/mongo-cxx-driver-r3.6.6.tar.gz
  tar -xzf mongo-cxx-driver-r3.6.6.tar.gz
  cd mongo-cxx-driver-r3.6.6/build
  
  cmake ..                            \
  -DCMAKE_BUILD_TYPE=Release          \
  -DCMAKE_INSTALL_PREFIX=/usr/local   \
  -DBSONCXX_POLY_USE_BOOST=1

  ```
- install mongodb [comunity edition](https://docs.mongodb.com/manual/tutorial/install-mongodb-on-ubuntu/) 
- good basic [guide](https://docs.mongodb.com/manual/tutorial/getting-started/)