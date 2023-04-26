
# TCP Non-blocking server

This is a simple Non-blocking server implementation in C++ using poll. It support uploading and downloading files from and to the server. Client needs to provide a valid header before an action is taken place. This repo also provides a compatible client.

## Server

Server works in an infinite loop and waits for connections. It is capable of running multiple connections at the same time. After a connection is established, it waits for a header that contains the action, filename and if the client is uploading, size of the file to be sent. If client is downloading, server sends size of the file first, then the file contents. After successful transfer server awaits for another header from the client and the cycle repeats.

**Example usage:**

    server IP_Address PORT
Use `0.0.0.0` for IP if you want to listen on all interfaces.
  

## Client

Client supports both upload and download from the server. It prepares a right header for the server and then sends or receives data.


**Example usage:**

    client Server_IP_Address PORT
After successful connection to the server, user will be prompted to choose if they want to upload or download and provide a filename.