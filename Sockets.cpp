#include "Sockets.h"

Sockets::Sockets(Connection & connectIn): connection(connectIn){
}

void Sockets::setSD(){
  //add child sockets to set
  for (int i = 0 ; i < connection.sockets.size() ; i++){
    //socket descriptor
    sd = connection.sockets[i].sd;
    //if valid socket descriptor then add to read list
    if(sd > 0)
    FD_SET(sd , &readfds);
    //highest file descriptor number, need it for the select function
    if(sd > max_sd)
    max_sd = sd;
  }
}

void Sockets::handleMessageCON(int index){
  connection.sockets[index].ip = connection.address.sin_addr.s_addr;
  connection.sockets[index].port = message.data.connections[0].port;
  connection.sockets[index].hashCode = message.hashCode;
  strcpy(connection.sockets[index].name, message.data.name);
  //aus IP int wieder ein chararray machen:::
  struct in_addr inTest;
  inTest.s_addr = connection.sockets[index].ip;
  //std::cout << "CON: " << "name: " <<  message.data.name << "ip: " << inet_ntoa(inTest)  << " port: " <<  message.data.connections[0].port << std::endl;
  //send SYN
  memset(&message, 0, sizeof(message));
  // nur gucken wer da ist
  std::cout <<"Now connected to: " << std::endl;
  std::cout <<"-----------------------" << std::endl;
  for(int i = 0; i < connection.sockets.size();i++){
    std::cout << connection.sockets[i].name << std::endl;
  }
  // alle conncections durchgehen und adden falls nicht die eigene
  for(int i = 0; i < connection.sockets.size();i++){
    if(i > 0){
      message.data.connections[i-1].hasNext = 1;
    }
    //eigene connection wird mit ip und port 0 gesendet nur wichitg fuer hash
    if((connection.sockets[i].port == connection.sockets[index].port) && (connection.sockets[i].ip == connection.sockets[index].ip)){
      message.data.connections[i].port =0;
      message.data.connections[i].ip = 0;
      message.hashCode = connection.master.hashCode;
      message.type = SYN_TYPE;
      strcpy(message.data.name,connection.master.name);

    } else {
      //std::cout << "writing into synlist port: " << connection.sockets[i].port << "and ip: " << connection.sockets[i].ip<<"at synlist index: "<<i <<std::endl;
      message.data.connections[i].port =connection.sockets[i].port;
      message.data.connections[i].ip = connection.sockets[i].ip;
      message.hashCode = connection.master.hashCode;
      message.type = SYN_TYPE;
      strcpy(message.data.name,connection.master.name);
    }
  }
  int sendResult = send(sd,(char*)&message, sizeof(message), 0);
}

void Sockets::handleMessageSYN(int index){
  //get name from connection
  connection.sockets[index].hashCode = message.hashCode;
  //std::cout << "hashcode empfangen: " << message.hashCode << std::endl;
  strcpy(connection.sockets[index].name, message.data.name);
  std::cout <<"Name of new Connection: " << message.data.name << std::endl;
  //check if sockets are already in socketlist ip=ip port=port?

  for(int i = 0; i < 37; i++){
    //std::cout<< "synlist connection ip: "<<message.data.connections[i].ip << " port: "<<message.data.connections[i].port<< std::endl;
    for(int j = 0; j < connection.sockets.size(); j++){
      if((connection.sockets[j].ip ==message.data.connections[i].ip) && (connection.sockets[j].port ==message.data.connections[i].port)){
        sdAlreadyListed = true;
        //  std::cout << "connection already there nothing to do"<< std::endl;
      }
    }
    //when no match then socket can be added since its new
    if(!sdAlreadyListed && (message.data.connections[i].ip!= 0)){
      if(connection.master.port == message.data.connections[i].port){
      } else {
        //std::cout << "synlist connection nr "<< i << " ip: "<<message.data.connections[i].ip<< " port: "<< message.data.connections[i].port<<  std::endl;
        //  std::cout << "connecting to port: "<< message.data.connections[i].port << std::endl;
        struct clientSocket newSocket;
        //transform ip from byte to char* to connect
        char* ip = new char[20];
        struct in_addr in;
        in.s_addr = message.data.connections[i].ip;
        ip = inet_ntoa(in);

        int testSd = connection.connect_socket(message.data.connections[i].port,ip);
        if(testSd > 0){
          char nameOfConnection[16];
          strcpy(nameOfConnection,connection.master.name);

          newSocket.sd = testSd;
          newSocket.ip = message.data.connections[i].ip;
          newSocket.port = message.data.connections[i].port;
          connection.sockets.push_back(newSocket);

          // my data for connected one
          memset(&message, 0, sizeof(message));
          message.data.connections[0].port = connection.master.port;
          strcpy(message.data.name, nameOfConnection);
          message.hashCode = connection.master.hashCode;
          message.type = CON_TYPE;
          int sendResult = send(testSd,(char*)&message, sizeof(message), 0);
        }
      }
    }
    if(message.data.connections[i].hasNext==0){
      //not next pls stop
      break;
    } else {
      //has next go further
    }
    sdAlreadyListed = false;
  }
}

void Sockets::handleMessgageMSG(int index){
  //getpeername(sd , (struct sockaddr*)&connection.address , (socklen_t*)&connection.addrlen);
  //std::cout<<inet_ntoa(connection.address.sin_addr)<<" : "<< ntohs(connection.address.sin_port)<< ">> " << message.data.text << std::endl;
  std::cout<<connection.sockets[index].name<<  ": " << message.data.text << std::endl;
}

void Sockets::programmThread(){
  while(TRUE)
  {
    //clear the socket set
    FD_ZERO(&readfds);
    //add master socket to set
    FD_SET(connection.master.sd, &readfds);
    max_sd = connection.master.sd;
    setSD();
    //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
    activity = select( max_sd + 1 , &readfds , NULL , NULL ,NULL);

    if ((activity < 0) && (errno!=EINTR)){
      printf("select error");
    }
    //If something happened on the master socket , then its an incoming connection
    if (FD_ISSET(connection.master.sd, &readfds)){
      if ((new_socket = accept(connection.master.sd, (struct sockaddr *)&connection.address, (socklen_t*)&connection.addrlen))<0){
        perror("accept");
        exit(EXIT_FAILURE);
      }
      //inform user of socket number - used in send and receive commands
      printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(connection.address.sin_addr) , ntohs(connection.address.sin_port));
      //add new socket to array of sockets
      struct clientSocket newSocket;

      newSocket.hashCode =connection.master.hashCode;
      newSocket.sd = new_socket;
      newSocket.ip = connection.address.sin_addr.s_addr;
      connection.sockets.push_back(newSocket);
    }

    //else its some IO operation on some other socket :)
    for (int i = 0; i < connection.sockets.size(); i++){

      sd = connection.sockets[i].sd;

      if (FD_ISSET( sd , &readfds)){
        memset(&message, 0, sizeof(message));
        //Check if it was for closing , and also read the incoming message
        if ((valread = recv( sd ,(char*)&message, sizeof(message), 0)) <= 0){
          //Somebody disconnected , get his details and print
          getpeername(sd , (struct sockaddr*)&connection.address , (socklen_t*)&connection.addrlen);
          printf("Host disconnected , ip %s , port %d, name %s \n" , inet_ntoa(connection.address.sin_addr) , ntohs(connection.address.sin_port),connection.sockets[i].name);
          //Close the socket and mark as 0 in list for reuse
          close( sd );
          connection.sockets.erase(connection.sockets.begin() + i);
        }
        //Read incoming message
        else{
          if(message.type == MSG_TYPE){
            handleMessgageMSG(i);
          } else if (message.type == SYN_TYPE){
            handleMessageSYN(i);
            //when a message of type con comes in -> get the port and name of that connection since its
            //its your current connection
          } else if(message.type == CON_TYPE){
            handleMessageCON(i);
          }
        }
      }
    }
  }
  std::cout << "ending socketThread safely\n";
}
