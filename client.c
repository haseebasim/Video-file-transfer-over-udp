# include <stdio.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <stdlib.h>
# include <errno.h>
# include <string.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <netinet/in.h>
# include <sys/stat.h>
# include <sys/sendfile.h>
# include <fcntl.h>
# include <time.h>

# define IP "127.0.0.1"
# define fileName "sample.mp4"
# define BUFSIZE 500 // Restricting payload 

struct packet {
	int seqNum;
	int size;
	char data[BUFSIZE];
};

//socket variables
int PORT;
int sock;
struct sockaddr_in socketAddress;
socklen_t addressLength = sizeof(struct sockaddr_in);


// File Variables
int sendlen;
int fileData;
int recvlen;
int file;
struct stat fileStat;
off_t fileSize;


// Segment Variables
struct packet packetArr[5]; // Restricting to 5 UDP segments 
int sequence = 0;
int totalAcks;
struct packet ack;
struct packet ackArr[5];
int window = 5;

void receiveAcks(){
    for(int i =0; i < window;i++){
            recvlen = recvfrom(sock, &ack,sizeof(struct packet),0,(struct sockaddr*) &socketAddress,&addressLength);

            //checking for any duplicates
            if(ackArr[ack.seqNum].size ==1){
                i--;
                continue;
            }
            
            //unique ack
            if(ack.size == 1){
                printf("Recieved Ack %d\n",ack.seqNum);
                
                ackArr[ack.seqNum]=ack;
                totalAcks++;
            }

        }
}




int main(int argc , char* argv[]){
    
    if(argc<2){
        printf("Port Number is not specified.\nPlease provide a port number.");
        return 0;
    }
    



    //assigning the port value obtained from the cli to PORT var
    PORT = atoi(argv[1]);//converting from string to int using atoi

    //creating socket
    sock = socket(AF_INET,SOCK_DGRAM,0);

    //validating socket creation
    if(sock < 0){
        printf("Socket could not be created.");
        return 0;
    }
    else{
        printf("Socket has been created.");
    }

    //initializing the values of the sockaddr_in struct
    memset((char* ) &socketAddress,0,sizeof(socketAddress));
    socketAddress.sin_family =AF_INET;
    socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddress.sin_port=htons(PORT);
    inet_pton(AF_INET,IP,&socketAddress.sin_addr);

    

    //opening file to read it only
    file = open(fileName, O_RDONLY);

    //checking if the file has been opened or not
    if(file<0){
        printf("There was an error opening the file. Please try again.");
        return 0;
    }

    //calculating size of the file
    fstat(file,&fileStat);
    fileSize = fileStat.st_size;
    printf("Size of the file is : %d bytes\n", (int)fileSize);

    //Sending the fileSize to the server
    sendlen = sendto(sock,&fileSize,sizeof(off_t),0,(struct sockaddr*) &socketAddress, addressLength);


    fileData = 1;


    //The loop will run till end of file
    while(fileData > 0){
        sequence=0;
        //storing file data in packets
        for(int i=0; i < window; i++){
            //reading file
            fileData = read(file,packetArr[i].data,BUFSIZE);

            packetArr[i].seqNum = sequence;
            packetArr[i].size = fileData;
            sequence++;

            //for last packet
            if(fileData ==0){       
                printf("End of file reached\n");
                packetArr[i].size = -999;
                window = i + 1;
                break;
            }
        }

        //sending packets to the server
        for(int i =0; i<window;i++){
            printf("Sending Packet %d\n",packetArr[i].seqNum);
            sendlen = sendto(sock,&packetArr[i],sizeof(struct packet),0,(struct sockaddr*) &socketAddress,addressLength);
        }

        //assigning the ackArr elements sizes to 0 
        memset(ackArr,0,sizeof(ackArr));
        for (int i = 0; i < window; i++)
        {
            ackArr[i].size = 0;
        }

        totalAcks = 0;
        
        
        receiveAcks();
        
        //Resending the packet for which no ack was recieved
        while(totalAcks != window){
        for (int i = 0; i < window; i++) {

			if (ackArr[i].size == 0) {
                printf("Sending missing packet: %d\n",packetArr[i].seqNum);
				sendlen = sendto(sock, &packetArr[i], sizeof(struct packet), 0, (struct sockaddr*) &socketAddress, addressLength);		
			}
		}

        receiveAcks();
    }


    }

    printf("\nFile has been sent successfully.\n");

    close(sock);
    return 0;
}
