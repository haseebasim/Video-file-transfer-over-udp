# include <stdio.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <errno.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <netinet/in.h>
# include <fcntl.h>
# include <pthread.h>
# include <fcntl.h>

#define recFileName "recievedfile.mp4"
#define BUFSIZE 500


struct packet{
    int seqNum;
    int size;
    char data[BUFSIZE];
};

// Socket Variables
int PORT;
int socketBind;
int sock;
struct sockaddr_in socketAddress;
socklen_t addressLength = sizeof(struct sockaddr_in);

// File Variables
int recvlen = 0;
int sendlen = 0;
int fd;
int fileSize;
int remainingData = 0;
int receivedData = 0;

// Segment Variables
int window= 5; // Number of packets to be sent at a time
struct packet packet;
struct packet packetArr[5];
int totalAcks;
struct packet ackArr[5];
int num;

void recvPackets (){
    //loop for receiving packets
    for (int i = 0; i < window; i++)
        {   
            //receiving packets from the client
            recvlen = recvfrom(sock, &packet,sizeof(struct packet),0,(struct sockaddr*) &socketAddress,&addressLength);
            //checking for already received packets
            if(packetArr[packet.seqNum].size != 0){
                //reallocating
                packetArr[packet.seqNum] = packet;

                num = packet.seqNum;
                ackArr[num].size = 1;
                ackArr[num].seqNum = packetArr[num].seqNum;
                //sending ack for already received packets 
                sendlen = sendto(sock,&ackArr[num],sizeof(ackArr[num]),0,(struct sockaddr*) &socketAddress,addressLength);
                printf("Ack sent for duplicate packet %d\n",ackArr[num].seqNum);

                i--;
                continue;
            }

            //check for last packet
            if(packet.size == -999){
                printf("Last packet found \n");

                window = packet.seqNum + 1;
                printf("window at last packet %d\n",window);
            }
            
            //receiving the packets and storing them in order
            if(recvlen > 0){
                printf("Packet Received %d\n", packet.seqNum);
                packetArr[packet.seqNum]=packet;
            }
        }
}

void sendAcks(){
    //loop for sending acks
    for (int i = 0; i < window; i++)
        {
            num = packetArr[i].seqNum;
            
            if(ackArr[num].size != 1){//check for already sent acks
                if(packetArr[i].size != 0){//checking for any missing packet
                    ackArr[num].size = 1;
                    ackArr[num].seqNum = packetArr[i].seqNum;
                //sending acks to client
                    sendlen = sendto(sock,&ackArr[num],sizeof(ackArr[num]),0,(struct sockaddr*) &socketAddress, addressLength);
                    if(sendlen > 0){
                        totalAcks++;//incrementing the total acks sent
                        printf("Ack Sent %d\n",ackArr[packetArr[num].seqNum].seqNum);
                    }

                }
            }
        }
}

int main(int argc , char* argv[]){
    //checking for valid no of args
    if(argc < 2){
        printf("Port number is not speicifed.\nPlease provide a port number.");
    }


    //assigning port to PORT var to access the port 
    PORT = atoi(argv[1]);

    //creating socket
    sock = socket(AF_INET,SOCK_DGRAM,0);

    // validating socket creation
    if(sock<0){
        printf("Socket could not be created.");
        return 0;
    }
    else{
        printf("Socket has been created");
    }

    //assigning values to the socketAddress properties
    memset((char*) &socketAddress, 0, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port = htons(PORT);

    //binding the socket to listen from a port
    socketBind = bind(sock,(struct sockaddr*) &socketAddress,sizeof(socketAddress));

    //validating socket bounds
    if(socketBind < 0){
        printf("Could not bind the socket\n");
        return 0;
    }

    //opening the file or creating and then opening file.
    fd = open(recFileName,O_RDWR|O_CREAT, 0755);

    //receiving the size of the file from client
    recvlen= recvfrom(sock,&fileSize,sizeof(off_t),0, (struct sockaddr*) &socketAddress,&addressLength);

    printf("Size of the file that is to be received is %d byte\n",fileSize);


    recvlen = 1;
    remainingData = fileSize;

    // the loop will run untill all data has been received
    while (remainingData > 0 || (window==5))
    {

     // Setting the packet size to zero
		memset(packetArr, 0, sizeof(packetArr));
        for (int i = 0; i < 5; i++)
        { 
            packetArr[i].size = 0; 
        }

		// Setting the ack size to zero
        memset(ackArr, 0, sizeof(ackArr));
        for (int i = 0; i < 5; i++)
        { 
            ackArr[i].size = 0;
        }

        //receiving packets
        recvPackets();
        
        totalAcks =0;

        //sending acks for the packets received only
        sendAcks();

        //receiving missing packets and sending acks for them
        while (totalAcks != window)
        {
            recvPackets();
            sendAcks();
        }


        // Write packet's data into file
		for (int i = 0; i < window; i++) {

			// Data is present in the packets and its not the last packet
			if (packetArr[i].size != 0 && packetArr[i].size !=-999)
			{
				printf("Writing packet: %d\n", packetArr[i].seqNum);
				write(fd, packetArr[i].data, packetArr[i].size);
				remainingData = remainingData - packetArr[i].size;
				receivedData = receivedData + packetArr[i].size;
				
			}
		}

        //displaying the received and remaining data
		printf("Received data: %d bytes\nRemaining data: %d bytes\n", receivedData, remainingData);

    }

    printf("\n\nFile Received Successfully.\n\n");
    
    close(sock);
    return 0;
}