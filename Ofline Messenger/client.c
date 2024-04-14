#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void handleUserInput(int);

void error(const char *msg) 
{
    perror(msg);
    exit(0);
}

void sendToServer(int sockfd, const char *buffer) 
{
    int n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        error("Error writing to socket");
    }
}

void readFromServer(int sockfd, char *buffer) 
{
    int n = read(sockfd, buffer, 255);
    if (n < 0) {
        error("Error reading from socket");
    }
    printf("%s\n", buffer);
}

int strLenCmp(const char *buffer, const char *command) 
{
    return strncmp(buffer, command, strlen(command)) == 0;
}

void showLoginMenu() 
{
    printf("1. Show all users\n");
    printf("2. Message user\n");
    printf("3. See conversations\n");
    printf("4. Logout\n");
}

void sendOkToServer(int sockfd)
{
    sendToServer(sockfd, "ok");
}

void handleLogin(int sockfd)
{
    char buffer[256];
    while (1) {
        showLoginMenu();
        
        printf("Insert option (1-4): ");

        bzero(buffer, sizeof(buffer));
        fgets(buffer, sizeof(buffer) - 1, stdin);
        
        buffer[strcspn(buffer, "\n")] = 0;

        if(strLenCmp(buffer, "4")) 
        {
            printf("Logging out...\n");
            return;
        }

        sendToServer(sockfd, buffer);

        bzero(buffer, sizeof(buffer));
        readFromServer(sockfd, buffer);

        if(strLenCmp(buffer, "login::show_all_users"))
        {
            sendOkToServer(sockfd);

            while(1)
            {
                bzero(buffer, sizeof(buffer));
                readFromServer(sockfd, buffer);

                if(strLenCmp(buffer, "end_of_list"))
                {
                    break;
                }

                sendOkToServer(sockfd);
            }
        }

        if(strLenCmp(buffer, "login::message_user"))
        {
            sendOkToServer(sockfd);

            readFromServer(sockfd, buffer);

            bzero(buffer, sizeof(buffer));
            fgets(buffer, sizeof(buffer) - 1, stdin);
            buffer[strcspn(buffer, "\n")] = 0;

            sendToServer(sockfd, buffer);

            readFromServer(sockfd, buffer);

            bzero(buffer, sizeof(buffer));
            fgets(buffer, sizeof(buffer) - 1, stdin);
            buffer[strcspn(buffer, "\n")] = 0;

            sendToServer(sockfd, buffer);

            readFromServer(sockfd, buffer);
        }

        if(strLenCmp(buffer, "login::see_conversations"))
        {
            sendOkToServer(sockfd);
            
            while(1)
            {
                bzero(buffer, sizeof(buffer));
                readFromServer(sockfd, buffer);
                int exitWhile = 0;

                if(strLenCmp(buffer, "end_of_list"))
                {
                    printf("If you want to see a conversation, type the username of the other user. Otherwise, type 'back'\n");

                    bzero(buffer, sizeof(buffer));

                    fgets(buffer, sizeof(buffer) - 1, stdin);

                    buffer[strcspn(buffer, "\n")] = 0;

                    if(strLenCmp(buffer, "back"))
                    {
                        break;
                    }

                    sendToServer(sockfd, buffer);

                    bzero(buffer, sizeof(buffer));

                    readFromServer(sockfd, buffer);

                    if(strLenCmp(buffer, "login::see_conversations::show_messages"))
                    {
                        sendOkToServer(sockfd);

                        while(1)
                        {
                            bzero(buffer, sizeof(buffer));
                            readFromServer(sockfd, buffer);

                            if(strLenCmp(buffer, "end_of_list"))
                            {
                                exitWhile = 1;
                                break;
                            }

                            sendOkToServer(sockfd);
                        }
                    }
                }

                if(exitWhile)
                {
                    break;
                }

                sendOkToServer(sockfd);
            }
        }
    }
}


void handleServerResponse(int sockfd) {
    char buffer[256];
    bzero(buffer, 256);
    readFromServer(sockfd, buffer);

    if(strLenCmp(buffer, "Not a valid option"))
    {
        handleUserInput(sockfd);
    }

    // register
    if(strLenCmp(buffer, "Username already exists"))
    {
        handleUserInput(sockfd);
    }

    // register
    if(strLenCmp(buffer, "Error in registration"))
    {
        handleUserInput(sockfd);
    }

    // register
    if(strLenCmp(buffer, "Registration successful"))
    {
        handleUserInput(sockfd);
    }

    // login
    if(strLenCmp(buffer, "Invalid username or password"))
    {
        handleUserInput(sockfd);
    }

    // login
    if(strLenCmp(buffer, "Login successful")) 
    {
        handleLogin(sockfd);
    } 

    if
    (
        strLenCmp(buffer, "Enter username: ") || 
        strLenCmp(buffer, "Enter password: ")
    ) 
    {
        bzero(buffer, 256);
        fgets(buffer, 255, stdin);

        buffer[strcspn(buffer, "\n")] = 0;

        sendToServer(sockfd, buffer);
        handleServerResponse(sockfd);
    }
}

void showMainMenu() 
{
    printf("1. Register\n");
    printf("2. Login\n");
    printf("3. Exit\n");
}

void handleUserInput(int sockfd) 
{
    char buffer[256];

    showMainMenu();
    
    printf("Insert option (1-3): ");

    bzero(buffer, 256);
    fgets(buffer, 255, stdin);
    
    buffer[strcspn(buffer, "\n")] = 0;

    if(strLenCmp(buffer, "3")) 
    {
        printf("Exiting...\n");
        return;
    }

    sendToServer(sockfd, buffer);

    handleServerResponse(sockfd);
}

int main(int argc, char *argv[]) {
    int sockfd, port_number;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 3) 
    {
       fprintf(stderr,"Usage %s hostname port\n", argv[0]);
       exit(0);
    }

    port_number = atoi(argv[2]);
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) 
    {
        error("Error opening socket");
    }
    
    server = gethostbyname(argv[1]);
    
    if (server == NULL) 
    {
        fprintf(stderr, "Error, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    
    serv_addr.sin_port = htons(port_number);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        error("Error connecting");
    }

    handleUserInput(sockfd);

    close(sockfd);
    
    return 0;
}