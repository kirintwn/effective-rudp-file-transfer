# UDP Reliable File Transfer

>The program is a implementation of effective UDP Reliable File Transfer. 

## Tech
- Custom protocol with Packet Type and Timestamp
- Using socket timeout to detect and implement the re-transmission mechanism

## Specification
- Packet Type
    - ACK (Acknowledgement)
    - NACK (Negative Acknowledgement)
    - RTS (Request To Send)
    - SYN (Synchronize)
    - RNACK (Request a feedback of non-arrival packets)
    - RST (Reset)
    - FIN (Finished a file transfer)
    - DATA

## Usage
```
make
./sender <receiver IP> <receiver port> <file name>
./receiver <port>
```


