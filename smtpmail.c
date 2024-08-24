// MAIL SERVER
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

void send_response(int socket, int code, const char *message);
void receive_buffer(int socket, char *main_buffer);
void get_mail(int socket, char *main_buffer);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int my_port = atoi(argv[1]);

    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind to the specified port
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(my_port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error binding");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) < 0)
    {
        perror("Error listening");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    int server_newsocket;              // concurrent server
    struct sockaddr_in client_address; // client address
    int client_len;                    // length of client address

    printf("Mail server is listening on port %d...\n", my_port);
    fflush(stdout);

    while (1)
    {

        client_len = sizeof(client_address);

        // Accept a connection
        server_newsocket = accept(server_socket, NULL, NULL);
        if (server_newsocket < 0)
        {
            perror("Error accepting connection");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // Fork a child process to handle the client

        if (fork() == 0)
        {

            close(server_socket);

            // send the initial message 220 <domain name> Service Ready
            char domain_name[100];
            gethostname(domain_name, 100);
            char buffer[MAX_BUFFER_SIZE];
            buffer[0] = '<';
            buffer[1] = '\0';
            strcat(buffer, domain_name);
            strcat(buffer, "> Service Ready");
            send_response(server_newsocket, 220, domain_name);

            // receive the message from client HELO <domain>
            memset(buffer, 0, sizeof(buffer));
            receive_buffer(server_newsocket, buffer);

            // if the message is not HELO <SP> <domain> <CRLF>, then exit
            if (strncmp(buffer, "HELO", 4) != 0)
            {
                printf("HELO not received, exiting...\n");
                fflush(stdout);
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // send the success message 250 OK <message received>
            char new_buff[100];
            strcpy(new_buff, "OK ");
            strcat(new_buff, buffer);

            send_response(server_newsocket, 250, new_buff);
            memset(new_buff, 0, sizeof(new_buff));
            memset(buffer, 0, sizeof(buffer));

            // receive the message from client MAIL FROM: <username>
            receive_buffer(server_newsocket, buffer);

            // if the message is not MAIL FROM: <SP> <reverse-path> <CRLF>, then exit
            if (strncmp(buffer, "MAIL FROM:", 10) != 0)
            {
                printf("MAIL FROM not received, exiting...\n");
                fflush(stdout);
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // send the success message 250 OK <message received>
            memset(new_buff, 0, sizeof(new_buff));

            strcpy(new_buff, "OK ");
            strcat(new_buff, buffer);

            send_response(server_newsocket, 250, new_buff);
            memset(new_buff, 0, sizeof(new_buff));
            memset(buffer, 0, sizeof(buffer));

            // receive the message from client RCPT TO: <username> and check if the user exists(check the user.txt file)
            receive_buffer(server_newsocket, buffer);

            // if the message is not RCPT TO: <SP> <forward-path> <CRLF>, then exit
            if (strncmp(buffer, "RCPT TO:", 8) != 0)
            {
                printf("RCPT TO not received, exiting...\n");
                fflush(stdout);
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // get the username from the message(first remove all the spaces and then extract the username from RCPT TO: <username>)
            char username[100];
            int i = 0;
            while (buffer[i] != ':')
                i++;
            i += 2;
            int j = 0;
            while (buffer[i] != '\0')
            {
                username[j] = buffer[i];
                i++;
                j++;
            }
            // strip part from the username after @
            char *ptr = strstr(username, "@");
            int index = ptr - username;
            username[index] = '\0';

            // check if the user exists(ach line in the user.txt file contains a username followed by some spaces and the user's password)
            FILE *fp = fopen("user.txt", "r");
            if (fp == NULL)
            {
                perror("Error opening user.txt");
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            char line[100];
            int valid = 0;
            while (fgets(line, sizeof(line), fp) != NULL)
            {
                char *token = strtok(line, " ");
                if (strcmp(token, username) == 0)
                {
                    valid = 1;
                    break;
                }
            }

            fclose(fp);

            // if the user does not exist, then send the error message 550 No such user here
            if (valid == 0)
            {
                send_response(server_newsocket, 550, "No such user\n");
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // send the success message 250 OK <message received>
            for (int i = 0; i < 100; i++)
                new_buff[i] = '\0';

            strcpy(new_buff, "OK ");
            strcat(new_buff, buffer);

            send_response(server_newsocket, 250, new_buff);
            memset(new_buff, 0, sizeof(new_buff));
            memset(buffer, 0, sizeof(buffer));

            // receive the message from client DATA
            receive_buffer(server_newsocket, buffer);

            // if the message is not DATA <CRLF>, then exit
            if (strncmp(buffer, "DATA", 4) != 0)
            {
                printf("DATA not received, exiting...\n");
                fflush(stdout);
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // send the success message 3354 Enter mail, end with "." on a line by itself<CRLF>
            send_response(server_newsocket, 354, "Enter mail, end with \".\" on a line by itself");

            // now receive the multiline message from the client, until a line with a single dot is received
            char main_buffer[MAX_BUFFER_SIZE];

            for (int i = 0; i < MAX_BUFFER_SIZE; i++)
                main_buffer[i] = '\0';

            get_mail(server_newsocket, main_buffer);

            memset(buffer, 0, sizeof(buffer));

            // send the success message 250 OK <message received>
            for (int i = 0; i < 100; i++)
                new_buff[i] = '\0';

            strcpy(new_buff, "OK ");
            strcat(new_buff, "Message accepted for delivery");

            send_response(server_newsocket, 250, new_buff);

            memset(new_buff, 0, sizeof(new_buff));


            // check if the file mymailbox exist in the subdirectory of the user, if not then create it
            char user_directory[100];
            strcpy(user_directory, username);
            strcat(user_directory, "/");
            strcat(user_directory, "mymailbox.txt");

            fp = fopen(user_directory, "a+");
            if (fp == NULL)
            {
                perror("Error opening file");
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            //add the time and date to the message after the "Subject:"
            time_t t;
            time(&t);
            char time_date[100];
            strcpy(time_date, ctime(&t));
            time_date[strlen(time_date) - 1] = '\0';
            int line_count = 0;
            for (int i = 0; i < strlen(main_buffer); i++)
            {
                if (main_buffer[i] == '\n')
                    line_count++;
                if (line_count == 3) {
                    //Append the time and date to the message after the "Subject:"
                    char time_date_message[100];
                    strcpy(time_date_message, "Received: ");
                    strcat(time_date_message, time_date);
                    strcat(time_date_message, "\n");
                    //Shift the main_buffer to the right by i+1
                    memmove(main_buffer + i + 1 + strlen(time_date_message), main_buffer + i + 1, strlen(main_buffer) - i - 1);
                    //Copy the time_date_message to the main_buffer
                    strncpy(main_buffer + i + 1, time_date_message, strlen(time_date_message));
                    break;
                }
            }

            char message_to_be_appended[MAX_BUFFER_SIZE];
            strcpy(message_to_be_appended, main_buffer);
            memset(main_buffer, 0, sizeof(main_buffer));

            // now append the message to the file
            fprintf(fp, "%s", message_to_be_appended);

            fclose(fp);

            // receive the message from client QUIT
            receive_buffer(server_newsocket, buffer);

            // if the message is not QUIT <CRLF>, then exit
            if (strncmp(buffer, "QUIT", 4) != 0)
            {
                printf("QUIT not received, exiting...\n");
                close(server_newsocket);
                exit(EXIT_FAILURE);
            }

            // send the success message 221 <domain name> Service closing transmission channel
            for (int i = 0; i < 100; i++)
                new_buff[i] = '\0';

            strcpy(new_buff, domain_name);
            new_buff[1] = '\0';
            strcat(new_buff, " Service closing transmission channel");

            send_response(server_newsocket, 221, new_buff);

            // close the socket
            close(server_newsocket);
            exit(EXIT_SUCCESS);
        }
        else
        {
            close(server_newsocket);
        }
    }

    // Close the server socket
    close(server_socket);

    return 0;
}

void send_response(int socket, int code, const char *message)
{
    char response[MAX_BUFFER_SIZE];
    sprintf(response, "%d %s\r\n", code, message);
    send(socket, response, strlen(response), 0);
    memset(response, 0, sizeof(response));
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

void get_mail(int socket, char *main_buffer)
{
    ssize_t bytesRead;
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    char rec_buffer[MAX_BUFFER_SIZE];
    memset(rec_buffer, 0, sizeof(rec_buffer));

    // if the buffer does not contain /r/n, then continue receiving in rec_buff and append to main_buffer
    while ((bytesRead = recv(socket, rec_buffer, sizeof(rec_buffer), 0)) > 0)
    {
        if (strstr(rec_buffer, "\r\n") == NULL)
        {
            strcat(buffer, rec_buffer);
            memset(rec_buffer, 0, sizeof(rec_buffer));
            continue;
        }
        else
        {
            // if <CR><LF> is found
            while (strstr(rec_buffer, "\r\n") != NULL)
            {
                // append the part of the rec_buffer before <CR><LF> to the buffer
                char *ptr = strstr(rec_buffer, "\r\n");
                int index = ptr - rec_buffer;
                strncat(buffer, rec_buffer, index);

                // Shift the rec_buffer to the left by index+2
                memmove(rec_buffer, rec_buffer + index + 2, strlen(rec_buffer) - index - 2);

                // now add a \n to the end of the buffer and append it to the main_buffer
                strcat(buffer, "\n");
                strcat(main_buffer, buffer);

                // in case the buffer contains ./n now, then return
                if (strncmp(buffer, ".\n", 2) == 0)
                {
                    return;
                }
                else
                {
                    // else clear the buffer and rec_buffer and continue receiving
                    memset(buffer, 0, sizeof(buffer));
                }
            }
        }
    }
}
