# Disclaimer
This project works only for windows because calls to the __WIN32 API__ are made inside the code.  
The implementation uses bare sockets with __UDP__ and works only for __ipv4__.

# Demo
To run the demo clone this repository to your preferred directory and run one of the commands below.  
(cmd) ```g++ -std=c++20 -o main Unit.h Unit.cpp main.cpp -lWs2_32 && .\main.exe```  
(powershell) ```g++ -std=c++20 -o main Unit.h Unit.cpp main.cpp -lWs2_32; .\main.exe```  
  
This will run a program that sends mock data to ```127.0.0.1:7777``` and displays whether the data was received successfully. (you can change ```dest_addr``` and ```port``` inside ```main.cpp```)

# Usage
This project contain a class ```Unit``` which can be used to send and receive data.  
To send data use ```Unit::send``` (this function blocks until the data is sent).  
To receive data override the function ```Unit::receive``` (this will be called from a separate thread when data is received).  
