#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>
#define SIZE 1024

/* Function receives data and writes it to a file */
ssize_t write_file(FILE* fp, int sockfd, ssize_t bytes_to_recv)
{
  ssize_t n;
  char buffer[SIZE];
  ssize_t total = 0;
  ssize_t written;

  while (1) 
  {
    n = recv(sockfd, buffer, SIZE, 0);
    total += n;
    written = fwrite(buffer,1,n,fp);
    printf("Written: %d", written);
    bzero(buffer, SIZE);
    if (n <= 0 || total == bytes_to_recv)
    {
      break;
    }
  }

  return total;
}

/* Function sends buffer contents to socket and returns amount of bytes sent through a reference*/
int sendall(int s, char *buf, size_t *len)
{
    int total = 0;     
    int bytesleft = *len;
    int n;

    /* Keep sending data until everything is sent*/
    while(total != *len)
    {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1)
        {
          break;
        }
        total += n;
        bytesleft -= n;
        //printf("Sent packet: %d\n", n);
    }

    *len = total;
    bzero(buf, SIZE);
    return n==-1?-1:0;
}

/* Function reads from a file and sends it to a socket */
void send_file(FILE *fp, int sockfd)
{
  char data[SIZE] = {0};
  size_t temp = 0;
    
    while((temp = fread(data,1,SIZE,fp)))
    {
        if (sendall(sockfd,data,&temp) == -1)
        {
            perror("[-] Could not send a chunk of a file");
        }
    }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " IP_ADDRESS PORT" << std::endl;
    return 1;
  }

  char* ip = argv[1]; // Server IP address
  int port = atoi(argv[2]); // Server port

  int sockfd;
  struct sockaddr_in server_addr;
  FILE *fp;

  /* Create a socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
  {
    perror("[-] Error at creating a socket.");
    exit(1);
  }
  std::cout << "[+] Socket created successfully." << std::endl;

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = inet_addr(ip);

  std::string header;
  std::string filename;

  /* Attempt to connect to the server */
  if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("[-] Could not connect to the server.");
    close(sockfd);
    exit(1);
  }
  std::cout << "[+] Connection established." << std::endl;

  do{
    std::cout << "Choose action [DOWNLOAD/UPLOAD]:";
    std::cin >> header;
    std::cout << "Filename:";
    std::cin >> filename;
    /* Prepare a header for the server */
    std::ostringstream oss;
    oss << header << " " << filename;

    if (header == "UPLOAD") 
    {
      fp = fopen(filename.c_str(), "rb");
      if (fp == NULL) 
      {
        perror("[-] Could not read from the file!");
        exit(1);
      }
      
      /* Get size of the file */
      fseek(fp, 0, SEEK_END);
      int total_bytes = ftell(fp);
      rewind(fp);

      /* Append size to the header */
      oss << " " << total_bytes << "\n";
      
      std::string message = oss.str();
      char msg[SIZE];
      bzero(msg, SIZE);
      strcpy(msg, message.c_str());

      size_t header_bytes = message.size();

      if (sendall(sockfd, msg, &header_bytes))
      {
        perror("[-] Could not send the header");
        close(sockfd);
        exit(1);
      }
      std::cout << "[+] Header sent to the server: " << oss.str() << std::endl;

      /* Send file to the server */
      send_file(fp, sockfd);
      fclose(fp);
      fp = NULL;
      std::cout << "[+] File has been sent." << std::endl;
    }
    else if (header == "DOWNLOAD")
    {
      fp = fopen(filename.c_str(), "wb");
      if (fp == NULL) 
      {
        perror("[-] Couldn't save the file.");
        exit(1);
      }

      /* When downloading no additional info is required by the server, terminate header with a newline char */
      oss << "\n";
      
      std::string message = oss.str();
      char msg[SIZE];
      bzero(msg, SIZE);
      strcpy(msg, message.c_str());

      size_t header_bytes = message.size();

      /* Send the file*/
      if (sendall(sockfd, msg, &header_bytes))
      {
        perror("[-] Could not send the header");
        close(sockfd);
        exit(1);
      }

      std::cout << "[+] Header sent to the server: " << header << " File: " << filename.c_str();
      bzero(msg, SIZE);

      ssize_t bytes_recv = 0;
      ssize_t bytes_to_recv = 0;

      while(true)
      {
        ssize_t n = recv(sockfd,msg,SIZE-bytes_recv,0);
        bytes_recv += n;

        /* Check received data if it includes a filesize*/
        const auto pos_start = msg;
        const auto pos_end = pos_start + bytes_recv;
        const auto pos_nl = std::find(pos_start, pos_end, '\n');

        if (pos_nl != pos_end)
          {
          *pos_nl = ' ';
          const int left = bytes_recv - (pos_nl - pos_start) - 1; //Count data after the header
          fwrite(pos_nl + 1, sizeof(char), left, fp); //Save any additional data after the header that was sent in that packet
          std::stringstream ss(msg);
          bytes_to_recv = stoi(ss.str());
          std::cout << " To receive:" << bytes_to_recv << " Bytes" << std::endl;
          bytes_to_recv -= left;
          bytes_recv = left;
          break;
        }
      }
      
      /* Download the file */
      bytes_recv += write_file(fp,sockfd, bytes_to_recv);
      std::cout<< "Received: " << bytes_recv << " B" << std::endl;
      fclose(fp);
      fp = NULL;
      std::cout << "[+] Download complete." << std::endl; 
      
    }
    else
    {
      std::cout << "[-] Invalid header. Closing the connection." << std::endl;
    }
  } while(true);

  std::cout << "[+] Closing the connection." << std::endl;
  close(sockfd);

  return 0;
}
