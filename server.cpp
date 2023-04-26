#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <algorithm>
#include <map>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <csignal>

#define SIZE 1024 //Buffer in bytes
#define STATE_WAITING 1
#define STATE_UPLOADING 2
#define STATE_DOWNLOADING 3

struct client {
    std::string name;
    FILE * file;
    char state;
    std::string filepath;
    char buffer[SIZE];
    int bytes_counter;
    int total_bytes;

    client(std::string name) {
        this->name = name;
        this->state = STATE_WAITING;
        this->file = nullptr;
        this->bytes_counter = 0;
        this->total_bytes = 0;
    };

    client() {

    }

};

bool whilekiller = false;

void handler(int signal)
{
  printf("[-]\nExecution has been interrupted!.\n[-]Server is shutting down...\n");
  whilekiller = true;
}

int main(int argc, char* argv[])
{
  
  if (argc != 3) 
  {
    std::cerr << "Usage: " << argv[0] << " IP_ADDRESS PORT" << std::endl;
    return 1;
  }
  
  signal(SIGABRT, handler); // Critical error (i.e. libc)
  signal(SIGINT, handler); // CTRL+C keystroke 
  signal(SIGTERM, handler); // Process ended (i.e. kill)

  /* Set server IP and port */
  char *ip = argv[1]; //IP Serwera
  int port = atoi(argv[2]); //port serwera

  char str[INET_ADDRSTRLEN];
  int e;

  int sockfd, new_sock;
  struct sockaddr_in server_addr, new_addr;
  socklen_t addr_size;
  char buffer[SIZE];

  std::map<int, FILE*> fileofsocket; //map that keeps track of files associated to sockets
  std::map<int, char> stateofsocket; //map that keeps track of state of socket
  std::map<int, client> clients;
  std::vector<pollfd> pfds;

  /* Creating a socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) 
  {
    perror("[-]Couldn't create a socket(is something running on that port?)");
    exit(1);
  }
  printf("[+]Socket created successfully.\n");

  pollfd temppoll;
  temppoll.fd = sockfd;
  temppoll.events = POLLIN;
  pfds.push_back(temppoll);
  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  //server_addr.sin_addr.s_addr = inet_addr(ip);

  /* IP adress validation */
  if(inet_aton(ip, &server_addr.sin_addr) == 0)
  {
    printf("Incorrect IP adress!");
    exit(1);
  }

  /* Binding socket to the port */
  e = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if(e < 0) 
  {
    perror("[-]Failure at binding the port!");
    close(sockfd);
    exit(1);
  }
  
  printf("[+]Sucessfully binded.\n");

  /* Start listening for incoming connections on that port */
  if(listen(sockfd, 10) == 0)
    { 
          printf("[+]Listening for connections...\n");
    }
    else
    {
      perror("[-]Error at listening");
      close(sockfd);
      exit(1);
    }
  /* Infinite loop to keep the server alive */
  while(!whilekiller)
  {
    int poll_count = poll(pfds.data(),pfds.size(), -1);
    if(poll_count == -1)
    {
      perror("[-]Error at poll");
      
      /* Clean up and close connections and files(if opened) */
      for(int i=pfds.size()-1; i>=0; i--)
      {
        close(pfds[i].fd);
        if(i!=0)
          if(clients.at(pfds[i].fd).file != NULL)
            fclose(clients.at(pfds[i].fd).file);
        pfds.erase(pfds.begin()+i);
      }
      /* Close the program */
      exit(1);
    }

    /* Checking if any of the active connections has anything to transfer */
    for(int i=0; i<pfds.size(); i++) 
    {
      /* Checking if any of the clients is ready for data transfer */
      if(pfds[i].revents & POLLIN || pfds[i].revents & POLLOUT)
      {
        /* If server is ready to read, handle new connection */
        if(i == 0)
        {
          addr_size = sizeof(new_addr);
          /* Accept the new incomming connection */
          new_sock = accept(sockfd, (struct sockaddr*)&new_addr, &addr_size);
          inet_ntop(AF_INET, &(new_addr.sin_addr), str, INET_ADDRSTRLEN);
          /* Check whether the connection is valid */
          if(new_sock < 0)
          {
            perror("[-]Connection not accepted!");
          }
          else
          {
            pollfd temppoll2;
            temppoll2.fd = new_sock;
            temppoll2.events = POLLIN;

            client tempclient((std::string(str)));
            
            /* Throw some info about the conncetion to stdout */
            printf("[+]Connection accepted! (IP: %s)(ID: %d)\n", tempclient.name.c_str(), temppoll2.fd);

            /* Map the connection */
            pfds.push_back(temppoll2);
            clients.insert(std::pair<int, client>(new_sock, tempclient));
            memset(clients.at(new_sock).buffer, 0, SIZE);
            
          }
        }
        else
        {
            /* Handle data sent by clients */
            /* Header check */
            if(clients.at(pfds[i].fd).state == STATE_WAITING)
            {
              
              ssize_t bytes_recv = recv(pfds[i].fd, 
                                        clients.at(pfds[i].fd).buffer + clients.at(pfds[i].fd).bytes_counter, 
                                        SIZE-clients.at(pfds[i].fd).total_bytes, 0);
              
            //   printf("Received a chunk of data from: %s\n", clients.at(pfds[i].fd).name.c_str());

            /* If the connection was closed by client, delete it from the map */
              if(bytes_recv <= 0)
              {
                printf("[-] Client %s hung up...\n",clients.at(pfds[i].fd).name.c_str());
                close(pfds[i].fd);
                clients.erase(pfds[i].fd);
                pfds.erase(pfds.begin()+i);
                i--;
                continue;
              }
              else
              {
                clients.at(pfds[i].fd).bytes_counter += bytes_recv;

                const auto pos_start = clients.at(pfds[i].fd).buffer; // Beginning of the buffer
                const auto pos_end = pos_start + clients.at(pfds[i].fd).bytes_counter; // End of the buffer
                const auto pos_nl = std::find(pos_start, pos_end, '\n'); // Newline position

                /* Check if the header is valid */
                if (pos_nl != pos_end)
                {
                  const auto pos_sp = std::find(pos_start, pos_nl, ' ');

                  if (pos_sp == pos_nl)
                  {
                    printf("[-] Invalid header!\n");
                    close(pfds[i].fd);
                    clients.erase(pfds[i].fd);
                    pfds.erase(pfds.begin()+i);
                    i--;
                    continue;
                  }

                  printf("[-] Received a full header from: %s\n", clients.at(pfds[i].fd).name.c_str());

                  std::stringstream ss(clients.at(pfds[i].fd).buffer);
                  std::string command;
                  std::string filename;
                  
                  ss >> command >> filename;

                  const int left = clients.at(pfds[i].fd).bytes_counter - (pos_nl - pos_start) - 1; //
                  clients.at(pfds[i].fd).filepath = filename;

                  /* If uploading, check if the client sent size of the file */
                  if (command == "UPLOAD")
                      {
                    const auto pos_sp2 = std::find(pos_sp + 1, pos_nl, ' ');
                    if (pos_sp2 == pos_nl)
                    {
                      printf("[-] Invalid header!\n");
                      close(pfds[i].fd);
                      clients.erase(pfds[i].fd);
                      pfds.erase(pfds.begin()+i);
                      i--;
                      continue;
                    }

                    int bytes_of_file = 0;
                    ss >> bytes_of_file;

                    /* Open requested file */
                    clients.at(pfds[i].fd).file = fopen(clients.at(pfds[i].fd).filepath.c_str(),"wb");
                    /* If cannot open, close connection */
                    if(clients.at(pfds[i].fd).file == NULL)
                    {
                      close(pfds[i].fd);
                      clients.erase(pfds[i].fd);
                      pfds.erase(pfds.begin()+i);
                      perror("[-] Could not open the file!\n");
                      i--;
                      break;
                    }
                    else printf("[+]Saving file: %s\n", clients.at(pfds[i].fd).filepath.c_str());
                    /* Save any data after the header that was sent in the same packet */
                    size_t written = fwrite(pos_nl + 1, sizeof(char), left, clients.at(pfds[i].fd).file);
                    clients.at(pfds[i].fd).total_bytes = bytes_of_file;
                    clients.at(pfds[i].fd).bytes_counter = left;
                    pfds[i].events = POLLIN;
                    /* Changing states etc. */
                    clients.at(pfds[i].fd).state = STATE_UPLOADING;
                    printf("[+] Changing state of client: %s to STATE_UPLOAD\n", clients.at(pfds[i].fd).name.c_str());
                    printf("[+] Size of data to be sent: %d Bytes\n", clients.at(pfds[i].fd).total_bytes);
                    memset(clients.at(new_sock).buffer, 0, SIZE);
                  }
                  else if(command == "DOWNLOAD")
                  {
                    clients.at(pfds[i].fd).file = fopen(clients.at(pfds[i].fd).filepath.c_str(),"rb");
                    
                    /* If there's no such file on the server, close the connection */
                    if(clients.at(pfds[i].fd).file == NULL)
                    {
                      close(pfds[i].fd);
                      clients.erase(pfds[i].fd);
                      pfds.erase(pfds.begin()+i);
                      perror("[-] Could not open the file!\n");
                      i--;
                      break;
                    }
                    else printf("[+] Reading from file: %s\n", clients.at(pfds[i].fd).filepath.c_str());

                    /* Getting the size of the file in bytes */
                    fseek(clients.at(pfds[i].fd).file,0,SEEK_END); //Seek end
                    clients.at(pfds[i].fd).total_bytes = ftell(clients.at(pfds[i].fd).file); //Save amout of bytes to a variable
                    rewind(clients.at(pfds[i].fd).file); //Rewind the pointer to the start

                    /* Inform server that this socket is expecting data to be sent to */
                    pfds[i].events = POLLOUT;
                    /* Change states etc. */
                    clients.at(pfds[i].fd).state = STATE_DOWNLOADING;
                    printf("[+] Changing state of client: %s to STATE_DOWNLOAD\n", clients.at(pfds[i].fd).name.c_str());
                    clients.at(pfds[i].fd).bytes_counter = 0;
                    memset(clients.at(new_sock).buffer, 0, SIZE);
                    ssize_t bytes_to_buff = sprintf(clients.at(pfds[i].fd).buffer, "%d\n", clients.at(pfds[i].fd).total_bytes);
                    send(pfds[i].fd, clients.at(pfds[i].fd).buffer, bytes_to_buff, 0);
                  }
                  else
                  {
                    printf("[-] Client requested an invalid operation!\n");
                    close(pfds[i].fd);
                    clients.erase(pfds[i].fd);
                    pfds.erase(pfds.begin()+i);
                    i--;
                    break;
                  }
                }
              }
            }
            /* Handle data upload */
            else if(clients.at(pfds[i].fd).state == STATE_UPLOADING)
            {
              ssize_t n;
              size_t written;
              /* Receiving parts of the file */
              n = recv(pfds[i].fd, clients.at(pfds[i].fd).buffer, SIZE, 0);
              clients.at(pfds[i].fd).bytes_counter += n;
              if(clients.at(pfds[i].fd).bytes_counter <= clients.at(pfds[i].fd).total_bytes && n > 0)
              {
                /* Saving data to a file on server side */
                written = fwrite(clients.at(pfds[i].fd).buffer, 1, n, clients.at(pfds[i].fd).file); 

                /* When all the data was transfered sucessfully clean up and change client's state to expect a new header */
                if(clients.at(pfds[i].fd).bytes_counter == clients.at(pfds[i].fd).total_bytes)
                {
                  printf("[+] Received: %d Bytes\n", clients.at(pfds[i].fd).bytes_counter);
                  fclose(clients.at(pfds[i].fd).file);
                  clients.at(pfds[i].fd).bytes_counter = 0;
                  clients.at(pfds[i].fd).file = NULL;
                  clients.at(pfds[i].fd).total_bytes = 0;
                  clients.at(pfds[i].fd).filepath.clear();
                  clients.at(pfds[i].fd).state = STATE_WAITING;
                  printf("[+] Changing state of client: %s to STATE_WAITING\n", clients.at(pfds[i].fd).name.c_str());
                }
              }
              else
              {
                if(n==0)
                {
                  printf("[+] Client has ended the connection (ID: %d)\n", pfds[i].fd);
                }
                else if(clients.at(pfds[i].fd).bytes_counter > clients.at(pfds[i].fd).total_bytes)
                {
                  perror("[-] Client sent more data than declared\n");
                }
                else
                {
                  perror("[-] Error at receiving data\n");
                }
                close(pfds[i].fd);
                fclose(clients.at(pfds[i].fd).file);
                clients.erase(pfds[i].fd);
                pfds.erase(pfds.begin()+i);
                i--;
              }
            }
            /* Client sent a DOWNLOAD header and wants to download a file from the server */
            else if(clients.at(pfds[i].fd).state == STATE_DOWNLOADING)
            {
              ssize_t n;
              int rb;
              if(!feof(clients.at(pfds[i].fd).file))
              {
                rb = fread(clients.at(pfds[i].fd).buffer, 1, SIZE, clients.at(pfds[i].fd).file);
                
                if(rb <= 0)
                {
                  close(pfds[i].fd);
                  fclose(clients.at(pfds[i].fd).file);
                  clients.erase(pfds[i].fd);
                  pfds.erase(pfds.begin()+i);
                  i--;
                }
                
                int sd = send(pfds[i].fd, clients.at(pfds[i].fd).buffer, rb, 0);
                clients.at(pfds[i].fd).bytes_counter += sd;

                if(sd <= 0)
                {
                  close(pfds[i].fd);
                  fclose(clients.at(pfds[i].fd).file);
                  clients.erase(pfds[i].fd);
                  pfds.erase(pfds.begin()+i);
                  i--;
                }
                else if(sd < rb) fseek(clients.at(pfds[i].fd).file, sd - rb, SEEK_CUR);
              }
              else
              {
                printf("[+] Sent: %d Bytes\n", clients.at(pfds[i].fd).bytes_counter);
                fclose(clients.at(pfds[i].fd).file);
                clients.at(pfds[i].fd).bytes_counter = 0;
                clients.at(pfds[i].fd).file = NULL;
                clients.at(pfds[i].fd).total_bytes = 0;
                clients.at(pfds[i].fd).filepath.clear();
                clients.at(pfds[i].fd).state = STATE_WAITING;
                printf("[+] Changing state of client: %s to STATE_WAITING\n", clients.at(pfds[i].fd).name.c_str());
              }
            }
            else
            {
              printf("[-] Internal server error...");
            }
        }
      }
    }
  }
  
  /* Clean up in case of an interrupt */
  for(int i=pfds.size()-1; i>=0; i--)
  {
    close(pfds[i].fd);
    if(i!=0)
      if(clients.at(pfds[i].fd).file != NULL)
        fclose(clients.at(pfds[i].fd).file);
    pfds.erase(pfds.begin()+i);
  }
  
  return 1;
}
