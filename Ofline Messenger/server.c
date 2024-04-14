#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct User {
    int socketFd;
    char username[100];
    char password[100];
    int loggedIn;
};

typedef struct User User;

User users[100];
int usersCount = 0;

void error(const char *msg) 
{
    perror(msg);
    exit(1);
}

void sendToClient(int sock, const char *message) 
{
    if (write(sock, message, strlen(message)) < 0) 
    {
        perror("Error writing to socket");
    }
}

int readFromClient(int sock, char *buffer, int bufsize) 
{
    bzero(buffer, bufsize);
    int n = read(sock, buffer, bufsize - 1);
    if (n < 0) 
    {
        perror("Error reading from socket");
    }
    return n;
}

int strLenCmp(const char *buffer, const char *command) 
{
    return strncmp(buffer, command, strlen(command)) == 0;
}

int existsUserFile(const char *username) 
{
    FILE *file = fopen("users.txt", "r");

    if (!file) return 0;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char existingUser[100], existingPass[100];
        if (sscanf(line, "%s %s", existingUser, existingPass) == 2) {
            if (strcmp(existingUser, username) == 0) {
                fclose(file);
                return 1;
            }
        }
    }

    fclose(file);

    return 0;
}

int existsUser(const char* username) 
{
    for(int i = 0; i < usersCount; i++) 
    {
        if(strcmp(users[i].username, username) == 0) 
        {
            return 1;
        }
    }
    return 0;
}

int validateCredentialsFile(const char *username, const char *password) 
{
    FILE *file = fopen("users.txt", "r");

    if (!file) return 0;

    char line[256];

    while (fgets(line, sizeof(line), file)) {
        char existingUser[100], existingPass[100];
        if (sscanf(line, "%s %s", existingUser, existingPass) == 2) {
            printf("%s %s\n", existingUser, existingPass);
            printf("%s %s\n", username, password);
            if (strcmp(existingUser, username) == 0 && strcmp(existingPass, password) == 0) {
                fclose(file);
                return 1;
            }
        }
    }
    fclose(file);

    return 0;
}

void showAllUsers(int sock) 
{
    char buffer[100];

    sendToClient(sock, "login::show_all_users");

    readFromClient(sock, buffer, sizeof(buffer));

    for(int i = 0; i < usersCount; i++) 
    {
        if(users[i].loggedIn) 
        {
            snprintf(buffer, sizeof(buffer), "%s (online)", users[i].username);
        } 
        else 
        {
            snprintf(buffer, sizeof(buffer), "%s (offline)", users[i].username);
        }

        sendToClient(sock, buffer);

        readFromClient(sock, buffer, sizeof(buffer));
    }

    sendToClient(sock, "end_of_list");
}

void getCurrentDate(char *date, size_t size) {
    time_t now;
    struct tm *local;

    time(&now);
    local = localtime(&now);

    if (size >= 20) { // "YYYY-MM-DD HH:MM" + '\0'
        sprintf(
            date, 
            "%d-%02d-%02d %02d:%02d",
            local->tm_year + 1900,
            local->tm_mon + 1,
            local->tm_mday,
            local->tm_hour,
            local->tm_min
        );
    }
}

char* getMessageFileName(const char *sender, const char *receiver) 
{
    char *fileName = malloc(100);
    
    if (!fileName) {
        return NULL;
    }

    snprintf(fileName, 100, "%s_%s.txt", sender, receiver);
    if (access(fileName, F_OK) != -1) {
        return fileName;
    }

    snprintf(fileName, 100, "%s_%s.txt", receiver, sender);
    if (access(fileName, F_OK) != -1) {
        return fileName;
    }

    free(fileName); 
    return NULL;
}

void messageUser(int sock, const char *sender) 
{
    char buffer[256], username[100], message[256], currentDate[20];

    sendToClient(sock, "login::message_user");
    readFromClient(sock, buffer, sizeof(buffer));

    sendToClient(sock, "Enter the username of the user you want to message: ");
    readFromClient(sock, username, sizeof(username));

    sendToClient(sock, "Enter your message: ");
    readFromClient(sock, message, sizeof(message));

    char *filename = getMessageFileName(sender, username);

    if(!filename) 
    {
        filename = (char*)malloc(100);
        snprintf(filename, 100, "%s_%s.txt", sender, username);
    }

    FILE *file = fopen(filename, "a");

    if(!file)
    {
        sendToClient(sock, "Error sending message");
        return;
    }

    getCurrentDate(currentDate, sizeof(currentDate));

    fprintf(file, "[%s] %s: %s\n", currentDate, sender, message);
    
    fclose(file);

    sendToClient(sock, "Message sent");
}

void handleRegister(int sock, char* username) 
{
    char password[100];

    sendToClient(sock, "Enter username: ");
    readFromClient(sock, username, 100);

    if (existsUser(username)) 
    {
        sendToClient(sock, "Username already exists");
        return;
    } 

    sendToClient(sock, "Enter password: ");
    readFromClient(sock, password, 100);
 
    FILE *file = fopen("users.txt", "a");

    if(!file)
    {
        sendToClient(sock, "Error in registration");
        return;
    }

    fprintf(file, "%s %s\n", username, password);

    users[usersCount].socketFd = sock;
    strcpy(users[usersCount].username, username);
    strcpy(users[usersCount].password, password);
    users[usersCount].loggedIn = 1;
    usersCount++;

    fclose(file);
    
    printf("Registration successful for %s\n", username);
    
    sendToClient(sock, "Registration successful");
}

char* getUsernameBySocketFd(int socketFd) 
{
    for(int i = 0; i < usersCount; i++) 
    {
        if(users[i].socketFd == socketFd) 
        {
            return users[i].username;
        }
    }
}

void seeConversations(int socketFd) 
{
    char buffer[100];

    sendToClient(socketFd, "login::see_conversations");

    readFromClient(socketFd, buffer, sizeof(buffer));

    FILE *usersFile = fopen("users.txt", "r");

    if(!usersFile) 
    {
        sendToClient(socketFd, "Error reading conversations");
        return;
    }

    char* senderUsername = getUsernameBySocketFd(socketFd);

    char line[256];

    while(fgets(line, sizeof(line), usersFile)) 
    {
        char username[100], password[100];

        if(sscanf(line, "%s %s", username, password) == 2) 
        {
            char *filename = getMessageFileName(senderUsername, username);

            if(filename) 
            {
                sendToClient(socketFd, username);
                readFromClient(socketFd, buffer, sizeof(buffer));
            }
        }
    }

    fclose(usersFile);

    sendToClient(socketFd, "end_of_list");

    char username[100];

    readFromClient(socketFd, username, sizeof(username));

    printf("User %s wants to see the conversation with %s\n", senderUsername, username);

    sendToClient(socketFd, "login::see_conversations::show_messages");

    readFromClient(socketFd, buffer, sizeof(buffer));

    char *filename = getMessageFileName(senderUsername, username);

    FILE *conversationFile = fopen(filename, "r");

    if(!conversationFile) 
    {
        sendToClient(socketFd, "Error reading messages");
        return;
    }

    while(fgets(line, sizeof(line), conversationFile)) 
    {
        line[strcspn(line, "\n")] = 0;
        sendToClient(socketFd, line);
        readFromClient(socketFd, buffer, sizeof(buffer));
    }

    fclose(conversationFile);

    sendToClient(socketFd, "end_of_list");
}

void handlePostLogin(int socketFd, const char* username) 
{
    char buffer[256];
    
    while (1) 
    { 
        if(readFromClient(socketFd, buffer, sizeof(buffer)) <= 0) break;

        if (strLenCmp(buffer, "1")) 
        {
            showAllUsers(socketFd);
        } 
        else if (strLenCmp(buffer, "2")) 
        {
            messageUser(socketFd, username);
        } 
        else if (strLenCmp(buffer, "3"))
        {
            seeConversations(socketFd);
        }
        else 
        {
            sendToClient(socketFd, "Not a valid option");
        }
    }
}

int getUserIndexByUsername(const char* username) 
{
    for(int i = 0; i < usersCount; i++) 
    {
        if(strcmp(users[i].username, username) == 0) 
        {
            return i;
        }
    }
}

int validateCredentials(const char *username, const char *password) 
{
    for(int i = 0; i < usersCount; i++) 
    {
        if(strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) 
        {
            return 1;
        }
    }

    return 0;
}

void handleLogin(int sock, char* username) 
{
    char password[100];

    sendToClient(sock, "Enter username: ");
    readFromClient(sock, username, sizeof(username));

    sendToClient(sock, "Enter password: ");
    readFromClient(sock, password, sizeof(password));

    if(!validateCredentials(username, password)) 
    {
        sendToClient(sock, "Invalid username or password");
        return;
    }

    printf("Login successful for %s\n", username);

    int userIndex = getUserIndexByUsername(username);

    users[userIndex].socketFd = sock;
    users[userIndex].loggedIn = 1;

    sendToClient(sock, "Login successful");
    
    handlePostLogin(sock, username);
}

int getUserIndexBySocketFd(int socketFd) 
{
    for(int i = 0; i < usersCount; i++) 
    {
        if(users[i].socketFd == socketFd) 
        {
            return i;
        }
    }
}

void *handleClient(void *newsockfd_ptr) 
{
    int newsockfd = *((int*)newsockfd_ptr);
    free(newsockfd_ptr);

    char buffer[256];
    char username[100];

    while (1) {
        if(readFromClient(newsockfd, buffer, sizeof(buffer)) <= 0) break;

        if(strLenCmp(buffer, "1")) 
        {
            handleRegister(newsockfd, username);
        } 
        else if(strLenCmp(buffer, "2")) 
        {
            handleLogin(newsockfd, username);
        }
        else
        {
            sendToClient(newsockfd, "Not a valid option");
        }
    }

    printf("Client disconnected\n");

    int index = getUserIndexBySocketFd(newsockfd);
    users[index].loggedIn = 0;

    close(newsockfd);
    return NULL;
}

void readUsersFromFile() 
{
    FILE *file = fopen("users.txt", "r");

    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file)) 
    {
        char username[100], password[100];
        if (sscanf(line, "%s %s", username, password) == 2) 
        {
            strcpy(users[usersCount].username, username);
            strcpy(users[usersCount].password, password);
            users[usersCount].loggedIn = 0;
            usersCount++;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    int sockfd, port_number;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) 
    {
        fprintf(stderr,"Error, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
       error("Error opening socket");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    port_number = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_number);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        error("Error on binding");
    }

    listen(sockfd, 5);

    printf("Waiting for clients...\n");

    clilen = sizeof(cli_addr);

    readUsersFromFile();

    while(1) 
    {
        int *newsockfd_ptr = malloc(sizeof(int));

        if (newsockfd_ptr == NULL) 
        {
            error("Error allocating memory");
        }

        *newsockfd_ptr = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (*newsockfd_ptr < 0) {
            free(newsockfd_ptr);
            error("Error on accept");
        }

        printf("Client connected\n");

        pthread_t thread;

        if (pthread_create(&thread, NULL, handleClient, newsockfd_ptr) != 0) 
        {
            error("Error creating thread");
        }

        pthread_detach(thread);
    }


    close(sockfd);

    return 0; 
}
