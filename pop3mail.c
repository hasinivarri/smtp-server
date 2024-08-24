// POP3 MAIL SERVER
// ANWESHA DAS 21CS30007
// VARRI HASINI 21CS10075

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

#define MAX_BUFFER_SIZE 4000
#define MAX_SUBJECT_SIZE 50

void send_response(int socket, const char* code, const char *message);
void receive_buffer(int socket, char *main_buffer);


int main(int argc, char *argv[]){

    if(argc != 2){
        printf("Usage: %s <port_number>\n", argv[0]);
        exit(1);
    }

    int my_port = atoi(argv[1]);

    int server_socket, server_newsocket;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        printf("Error in creating socket\n");
        fflush(stdout);
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(my_port);

    if(bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        printf("Error in binding\n");
        fflush(stdout);
        exit(1);
    }

    if(listen(server_socket, 5) < 0){
        printf("Error in listening\n");
        fflush(stdout);
        exit(1);
    }

    struct sockaddr_in client_address; // client address
    int client_len;                    // length of client address

    printf("Mail server is listening on port %d...\n", my_port);
    fflush(stdout);

    while(1){


        client_len = sizeof(client_address);
        server_newsocket = accept(server_socket, (struct sockaddr *) &client_address, &client_len);
        if(server_newsocket < 0){
            printf("Error in accepting\n");
            fflush(stdout);
            exit(1);
        }

        if(fork() == 0){

            close(server_socket);

            //send greeting : +OK POP3 server ready
            char buffer[MAX_BUFFER_SIZE];
            memset(buffer, 0, sizeof(buffer));

            send_response(server_newsocket, "+OK", "POP3 server ready");
            memset(buffer, 0, sizeof(buffer));
            
            char username[100];
            char password[100];
            memset(username, 0, 100);
            memset(password, 0, 100);


            //authentication state
            while(1){
                
                //receive username
                receive_buffer(server_newsocket, buffer);
                if(strncmp(buffer, "QUIT", 4) == 0){
                    close(server_newsocket);
                    exit(0);
                }

                if(strncmp(buffer, "USER", 4) == 0){

                    //check if the username is valid
                    //check from the file if the username exists
                    FILE *file = fopen("user.txt", "r");
                    if(file == NULL){
                        send_response(server_newsocket, "-ERR", "Internal server error");
                        continue;
                    }

                    char line[500];
                    int found = 0;
                    while(fgets(line, sizeof(line), file)){
                        char *token = strtok(line, " ");
                        if(strcmp(token, buffer+5) == 0){
                            found = 1;
                            //store the username in the buffer username
                            strncpy(username, token, strlen(token));
                            //store the password for the username in the buffer password
                            token = strtok(NULL, " ");
                            //take the password after the space, and remove the newline character at its end
                            strncpy(password, token, strlen(token)-1);
                            strcat(password, "\0");
                            break;
                        }
                    }
                    fclose(file);

                    if(found == 0){
                        send_response(server_newsocket, "-ERR", "Invalid username");
                        continue;
                    }else{
                        send_response(server_newsocket, "+OK", "Username accepted");
                    }


                }else{
                    send_response(server_newsocket, "-ERR", "Invalid command");
                    continue;
                }

                //receive password
                memset(buffer, 0, sizeof(buffer));
                receive_buffer(server_newsocket, buffer);

                if(strncmp(buffer, "PASS", 4) == 0){
                        
                        if(strcmp(password, buffer+5) == 0){
                            send_response(server_newsocket, "+OK", "Password accepted");
                            break;
                        }else{
                            send_response(server_newsocket, "-ERR", "Invalid password");
                            continue;
                        }
                }

            }
            

            //transaction state
            //access the mailbox of the user
            //go to the folder usename and open the file mymailbox.txt
            char mailbox[100];
            memset(mailbox, 0, sizeof(mailbox));
            strcpy(mailbox, username);
            strcat(mailbox, "/");
            strcat(mailbox, "mymailbox.txt");

            FILE *file;

            //keep an array to_be_deleted to keep track of the mails to be deleted
            int to_be_deleted[1000];

            while(1){
                memset(buffer, 0, sizeof(buffer));
                receive_buffer(server_newsocket, buffer);

                if(strncmp(buffer, "STAT", 4) == 0){
                    fflush(stdout);
                    //return the number of mails and the total size of the mails
                    file = fopen(mailbox, "r");
                    if(file == NULL){
                        printf("Error in opening mailbox\n");
                    }
                    int mails = 0;
                    int size = 0;
                    char line[500];
                    while(fgets(line, sizeof(line), file)){
                        size += strlen(line);
                        if(strncmp(line, "From:", 5) == 0){
                            mails++;
                            if(to_be_deleted[mails-1] != 0){
                                mails--;
                                size -= strlen(line);
                                //skip to the end of the mail
                                while(fgets(line, sizeof(line), file)){
                                    if(strcmp(line, ".") == 0){
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    char response[100];
                    sprintf(response, "+OK %d %d", mails, size);
                    send_response(server_newsocket, response, "mails");


                    fclose(file);                    
                }else if(strncmp(buffer, "LIST", 4) == 0){
                    //first check whether te first argument is empty or not
                    file = fopen(mailbox, "r");
                    if(buffer[5] == '\0'){
                        //no arguments sent, so list all the mails(except the marked ones), in the form <mail_number> <size>
                        int num = 0;
                        int count = 0;
                        char mails[1000][MAX_BUFFER_SIZE];
                        char line[500];
                        while(fgets(line, sizeof(line), file)){
                            if(strncmp(line, "From:", 5) == 0){
                                count++;
                                if(to_be_deleted[count - 1] == 0){
                                    num++;
                                    //find out the line when this message ends
                                    int size = 0;
                                    while(fgets(line, sizeof(line), file)){
                                        size += strlen(line);
                                        if(strcmp(line, ".") == 0){
                                            break;
                                        }
                                    }
                                    sprintf(mails[num-1], "%d %d", count, size);
                                }
                            }
                            //if the line is the end of the file, then break
                            if(feof(file)){
                                break;
                            }
                        }
                        char msg[100];
                        memset(msg, 0, sizeof(msg));
                        sprintf(msg, "%d messages", num);
                        send_response(server_newsocket, "+OK", msg);

                        for(int i = 0; i < num; i++){
                            send_response(server_newsocket, "", mails[i]);
                        }
                    }else{
                        //if the first argument is not empty, then list the mail with the given number
                        int num = atoi(buffer+5);
                        char line[500];
                        int mails = 0;
                        while(fgets(line, sizeof(line), file)){
                            if(strncmp(line, "From:", 5) == 0){
                                mails++;
                                if(mails == num){
                                    if(to_be_deleted[mails-1] != 0){
                                        send_response(server_newsocket, "-ERR", "Mail marked for deletion");
                                    }else{
                                        int size = 0;
                                        while(fgets(line, sizeof(line), file)){
                                            size += strlen(line);
                                            if(strcmp(line, ".") == 0){
                                                break;
                                            }
                                        }
                                        char response[100];
                                        sprintf(response, "%d %d", num, size);
                                        send_response(server_newsocket, "+OK", response);
                                    }
                                }
                            }
                        }
                    }
                    fclose(file);

                }else if(strncmp(buffer, "RETR", 4) == 0){
                    fflush(stdout);
                    //return the mail with the given number
                    file = fopen(mailbox, "r");
                    int mail_number = atoi(buffer+5);
                    char line[500];
                    int mails = 0;
                    while(fgets(line, sizeof(line), file)){
                        if(strncmp(line, "From:", 5) == 0){
                            mails++;
                            if(mails == mail_number){
                                if(to_be_deleted[mail_number-1] != 0){
                                    send_response(server_newsocket, "-ERR", "Mail marked for deletion");
                                }else{
                                    send_response(server_newsocket, "+OK", "Message sent below");
                                    //Remove the newline character at the end of the line
                                    line[strlen(line)-1] = '\0';
                                    send_response(server_newsocket, "", line);
                                    while(fgets(line, sizeof(line), file)){
                                        line[strlen(line)-1] = '\0';
                                        send_response(server_newsocket, "", line);
                                        if(strcmp(line, ".") == 0){
                                            break;
                                        }
                                    }
                                }
                            }
                        }else{
                            continue;
                        }
                    }
                    fclose(file);
                }else if(strncmp(buffer, "DELE", 4) == 0){
                    //mark the mail with the given number for deletion
                    int mail_number = atoi(buffer+5);
                    to_be_deleted[mail_number-1] = 1;
                    send_response(server_newsocket, "+OK", "Mail marked for deletion");
                }else if(strncmp(buffer, "RSET", 4) == 0){
                    //unmark all the mails marked for deletion
                    memset(to_be_deleted, 0, sizeof(to_be_deleted));
                    send_response(server_newsocket, "+OK", "All mails unmarked");
                }else if(strncmp(buffer, "QUIT", 4) == 0){
                    //update state
                    //delete all the mails marked for deletion

                    file = fopen(mailbox, "r");
                    //open a temp file in the folder username and write all the mails except the ones marked for deletion
                    char temp_file[100];
                    memset(temp_file, 0, sizeof(temp_file));
                    strcpy(temp_file, username);
                    strcat(temp_file, "/temp.txt");
                    FILE *temp = fopen(temp_file, "w");
                    // printf("Temp file: %s\n", temp_file);
                    // fflush(stdout);
                    if(temp == NULL){
                        send_response(server_newsocket, "-ERR", "Internal server error");
                        close(server_newsocket);
                        exit(1);
                    }
                    char line[500];
                    int mails = 0;
                    while(fgets(line, sizeof(line), file)){
                        if(strncmp(line, "From:", 5) == 0){
                            mails++;
                            if(to_be_deleted[mails-1] != 0){
                                //skip the mail
                                continue;
                            }else{
                                //write the mail to the temp file
                                fputs(line, temp);
                                while(fgets(line, sizeof(line), file)){
                                    fputs(line, temp);
                                    if(strcmp(line, ".") == 0){
                                        break;
                                    }
                                }
                            }
                        
                        }else{
                            if(feof(file)){
                                break;
                            }
                            continue;
                        }  
                    }
                    // printf("Mails: %d\n", mails);
                    fclose(file);
                    //copy the contents of the temp file to the mailbox file
                    fclose(temp);              
                    // printf("Mailbox: %s\n", mailbox);
                    fflush(stdout);
                    rename(temp_file, mailbox);

                    send_response(server_newsocket, "+OK", "Goodbye");
                    close(server_newsocket);
                    exit(0);
                } else {
                    printf("Invalid command\n");
                    fflush(stdout);
                    while (1);
                }   
            }   
        }
    }
}

void receive_buffer(int socket, char *main_buffer)
{
    ssize_t bytesRead;
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    while ((bytesRead = recv(socket, buffer, sizeof(buffer), 0)) > 0)
    {
        // if <CR><LF> is not found, then continue receiving
        if (strstr(buffer, "\r\n") == NULL)
        {
            strcat(main_buffer, buffer);
            for (int i = 0; i < MAX_BUFFER_SIZE; i++)
                buffer[i] = '\0';
            continue;
        }
        else
        {
            // if <CR><LF> is found, then append the part of the buffer before <CR><LF> to the main buffer and return
            char *ptr = strstr(buffer, "\r\n");
            int index = ptr - buffer;
            strncat(main_buffer, buffer, index);
            return;
        }
    }
}

void send_response(int socket, const char* code, const char *message)
{
    char response[MAX_BUFFER_SIZE];
    if(strcmp(code, "")!= 0)
    {
        sprintf(response, "%s %s\r\n", code, message);
        send(socket, response, strlen(response), 0);
        memset(response, 0, sizeof(response));
    }else if(strcmp(code, "") == 0){
        
        sprintf(response, "%s\r\n", message);
        send(socket, response, strlen(response), 0);
        memset(response, 0, sizeof(response));
    }
}