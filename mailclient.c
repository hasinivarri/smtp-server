// MAIL CLIENT
// ANWESHA DAS 21CS30007
// VARRI HASINI 21CS10075

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 4000
#define MAX_SUBJECT_SIZE 50

void send_mail(int client_socket, int smtp_port, const char *server_IP);
void receive_mail(int client_socket, int pop3_port, const char *server_IP, char *username, char *password);
void receive_buffer(int socket, char *main_buffer);
void get_mail(int socket, char* main_buffer, int mail_num);

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <server_IP> <smtp_port> <pop3_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_IP = argv[1];
    int smtp_port = atoi(argv[2]);
    int pop3_port = atoi(argv[3]);

    char username[100];
    char password[100];
    printf("Enter username: ");
    fflush(stdout);
    scanf(" %[^\n]", username);
    printf("Enter password: ");
    fflush(stdout);
    scanf(" %[^\n]", password);

    while (1)
    {

        // Create socket
        int client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0)
        {
            perror("Error creating socket");
            exit(EXIT_FAILURE);
        }
        // Now, you can implement a loop to continuously take user input
        int option;

        printf("Options:\n");
        printf("1. Manage mail\n");
        printf("2. Send mail\n");
        printf("3. Exit\n");
        printf("Enter option: ");
        fflush(stdout);
        scanf("%d", &option);

        if (option == 1)
        {
            receive_mail(client_socket, pop3_port, server_IP, username, password);
        }
        else if (option == 2)
        {
            send_mail(client_socket, smtp_port, server_IP);
        }
        else if (option == 3)
        {
            printf("Quitting...\n");
            fflush(stdout);
            break;
        }
        else
        {
            printf("Invalid option\n");
            fflush(stdout);
            continue;
        }
    }

    return 0;
}

void receive_mail(int client_socket, int pop3_port, const char *server_IP, char *username, char *password)
{

    // connect to POP3 server
    //  Connect to the SMTP server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(pop3_port);

    if (inet_pton(AF_INET, server_IP, &server_address.sin_addr) <= 0)
    {
        perror("Invalid server IP address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error connecting to the server");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // receive greeting +OK POP3 server ready
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "+OK", 3) != 0)
    {
        printf("Error in receiving greeting...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }

    // authentication state
    //  Send the username and password to the server for authorisation
    char auth_buffer[MAX_BUFFER_SIZE];
    memset(auth_buffer, 0, sizeof(auth_buffer));
    strcpy(auth_buffer, "USER ");
    strcat(auth_buffer, username);
    strcat(auth_buffer, "\r\n");
    send(client_socket, auth_buffer, strlen(auth_buffer), 0);
    memset(auth_buffer, 0, sizeof(auth_buffer));

    // Receive the message from server +OK
    receive_buffer(client_socket, auth_buffer);

    if (strncmp(auth_buffer, "+OK", 3) != 0)
    {
        if (strncmp(auth_buffer, "-ERR", 4) == 0)
        {
            printf("Invalid username...\n");
            fflush(stdout);
            return;
        }
        else
        {
            printf("Error in USER authentication...\n");
            fflush(stdout);
            return;
        }
    }

    memset(auth_buffer, 0, sizeof(auth_buffer));
    strcpy(auth_buffer, "PASS ");
    strcat(auth_buffer, password);
    strcat(auth_buffer, "\r\n");
    send(client_socket, auth_buffer, strlen(auth_buffer), 0);

    memset(auth_buffer, 0, sizeof(auth_buffer));
    // receive the +OK message
    receive_buffer(client_socket, auth_buffer);

    if (strncmp(auth_buffer, "+OK", 3) != 0)
    {
        if (strncmp(auth_buffer, "-ERR", 4) == 0)
        {
            printf("Invalid password...\n");
            fflush(stdout);
            return;
        }
        else
        {
            printf("Error in PASS authentication...\n");
            fflush(stdout);
            return;
        }
    }

    int to_be_deleted[1000];
    //initialise all to 0
    for(int i = 0; i< 1000; i++){
        to_be_deleted[i] = 0;
    }

    int to_see;

    // transaction state
    while (1)
    {
        fflush(stdout);
        // get the number of mails in the mailbox
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "STAT\r\n");
        send(client_socket, buffer, strlen(buffer), 0);

        memset(buffer, 0, sizeof(buffer));
        receive_buffer(client_socket, buffer);

        if (strncmp(buffer, "+OK", 3) != 0)
        {
            if (strncmp(buffer, "-ERR", 4) == 0)
            {
                printf("Error in STAT...\n");
                fflush(stdout);
                send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
                return;
            }
        }

        // get the number of mails
        char *token = strtok(buffer, " ");
        token = strtok(NULL, " ");
        int num_mails = atoi(token);
        int count = 0;

        char mails[num_mails][MAX_BUFFER_SIZE];

        // get the details of each mail
        int i = 0;
        int non_marked_mails[1000];
        memset(non_marked_mails, 0, sizeof(non_marked_mails));
        while(count < num_mails){
            i++;
            if(to_be_deleted[i-1] == 0){
                non_marked_mails[count] = i; //non_marked_mail is 0 indexed but stores 1 indexed mail numbers
                get_mail(client_socket, mails[count], i); //count is 0 indexed, mails is 0 indexed, i is 1 indexed
                count++; //count is now 1 indexed
            }
        }

        // display the details of each mail in the format: Sl. No. <Sender’s email id> <When received, in date : hour : minute> <Subject>
        printf("Sl. No.\t\tSender's email id\t\tWhen received\t\tSubject\n");
        count = 1;

        for(int j = 0; j < i; j++){ //j is 0 indexed
            if(to_be_deleted[j] == 0){
                printf("%d.\t", count); //count is 1 indexed
                fflush(stdout);  

                char tempmail[MAX_BUFFER_SIZE];
                strcpy(tempmail, mails[j]);

                //Tokenize the mail[j] to get the sender's email id, when received and subject
                char *token = strtok(tempmail, "\n");
                // token = strtok(NULL, "\n");

                //Sender's email id
                token = strtok(NULL, "\n");
                printf("%s\t\t", token);
                fflush(stdout);

                //Subject
                char sub[MAX_SUBJECT_SIZE];
                token = strtok(NULL, "\n");
                token = strtok(NULL, "\n");
                strcpy(sub, token);

                //When received
                token = strtok(NULL, "\n");
                printf("%s\t\t", token);
                fflush(stdout);

                //print the subject
                printf("%s\n", sub);
                
                count++; //count is 1 indexed

            }else{
                continue;
            }
        }

        getchar();
        printf("\nEnter mail number to see: ");
        fflush(stdout);
        char temp_input_buf[10];
        memset(temp_input_buf, 0, sizeof(temp_input_buf));
        fgets(temp_input_buf, 10, stdin);
        to_see = atoi(temp_input_buf);
        
        //if the number is -1, go to the previous menu, else if out of range, promt to read again
        while(1){
            if(to_see == -1){
                break;
            }else if(to_be_deleted[to_see - 1] == 1){
                printf("Mail already deleted, enter again: ");
                fflush(stdout);
                memset(temp_input_buf, 0, sizeof(temp_input_buf));
                fgets(temp_input_buf, 10, stdin);
                to_see = atoi(temp_input_buf);
            }
            else if(to_see < 1 || to_see > num_mails){
                printf("Invalid mail number, enter again: ");
                fflush(stdout);
                memset(temp_input_buf, 0, sizeof(temp_input_buf));
                fgets(temp_input_buf, 10, stdin);
                to_see = atoi(temp_input_buf);
            }
            else{
                break;
            }
        }

        if(to_see == -1){
            break;
        }

        //display the particular mail from the "From: " line
        //tokenise the mail[to_see - 1] in terms of \n
        token = strtok(mails[to_see - 1], "\n");
        token = strtok(NULL, "\n");

        if (token == NULL)
        {
            printf("Error in tokenising...\n");
            fflush(stdout);
        }

        while(strcmp(token, ".") != 0) {
            printf("%s\n", token);
            fflush(stdout);
            token = strtok(NULL, "\n");
        }

        char c = getchar(); //wait for any character
        if(c == 'd'){
            //mark this mail to be deleted
            memset(buffer, 0, sizeof(buffer));
            strcpy(buffer, "DELE ");
            char mail_id[10];

            //find the correct mail number using the non_marked_mails array
            memset(mail_id, 0, sizeof(mail_id));
            sprintf(mail_id, "%d", non_marked_mails[to_see - 1]);

            strcat(buffer, mail_id);
            
            strcat(buffer, "\r\n");
            send(client_socket, buffer, strlen(buffer), 0);

            memset(buffer, 0, sizeof(buffer));
            receive_buffer(client_socket, buffer);

            to_be_deleted[to_see - 1] = 1;

            if (strncmp(buffer, "+OK", 3) != 0)
            {
                printf("Error in DELE...\n");
                fflush(stdout);
                send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
                return;
            }
        }else{
            continue;
        }
    }

    if(to_see == -1){
        // quit the session
        
        memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "QUIT\r\n");
        send(client_socket, buffer, strlen(buffer), 0);

        memset(buffer, 0, sizeof(buffer));
        receive_buffer(client_socket, buffer);

        if (strncmp(buffer, "+OK", 3) != 0)
        {
            printf("Error in QUIT...\n");
            fflush(stdout);
            return;
        }

        return;
    }
}

void send_mail(int client_socket, int smtp_port, const char *server_IP)
{
    // Connect to the SMTP server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(smtp_port);

    if (inet_pton(AF_INET, server_IP, &server_address.sin_addr) <= 0)
    {
        perror("Invalid server IP address");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Error connecting to the server");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char domain_name_receiver[100];

    // Receive the message from server 220 <domain name> Service Ready
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    receive_buffer(client_socket, buffer);

    // if the message is not 220 <domain name> Service Ready, then keep waiting for the message
    while (strncmp(buffer, "220", 3) != 0)
    {
        receive_buffer(client_socket, buffer);
    }

    // get the domain name of the receiver (buffer is of the form 220 <domain name> Service Ready)
    char *token = strtok(buffer, " ");
    token = strtok(NULL, " ");
    strcpy(domain_name_receiver, token);

    // now send the acknowledgement message
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "HELO ");
    strcat(buffer, domain_name_receiver);
    strcat(buffer, "\r\n");

    send(client_socket, buffer, strlen(buffer), 0);

    // get the success message from the server, if something else received, exit
    memset(buffer, 0, sizeof(buffer));
    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "250", 3) != 0)
    {
        printf("Error in HELO 250 OK...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }

    // Read the mail details from the user
    printf("\nEnter the mail details in the specified format:\n");
    printf("From: \n");
    printf("To: \n");
    printf("Subject: \n");
    printf("Message body (enter '.' on a line by itself to end):\n");
    fflush(stdout);

    // take line by line input from user
    char from[100];
    char to[100];
    char subject[MAX_SUBJECT_SIZE];
    char message_body[MAX_BUFFER_SIZE * 60];

    // scan one line at a time and check if it is in the correct format
    scanf(" %[^\n]", buffer);
    if (strncmp(buffer, "From: ", 6) != 0)
    {
        printf("Error: Incorrect order for From field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    strcpy(from, buffer);

    memset(buffer, 0, sizeof(buffer));

    scanf(" %[^\n]", buffer);
    if (strncmp(buffer, "To: ", 4) != 0)
    {
        printf("Error: Incorrect order for To field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    strcpy(to, buffer);

    scanf(" %[^\n]", buffer);
    if (strncmp(buffer, "Subject: ", 9) != 0)
    {
        printf("Error: Incorrect order for Subject field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    strcpy(subject, buffer);

    // read the message body(it can contain \n, it ends when a \n.\n is encountered)
    while (1)
    {
        // read one line at a time
        memset(buffer, 0, sizeof(buffer));
        scanf(" %[^\n]", buffer);

        // if the line is ., then break
        if (strcmp(buffer, ".") == 0)
        {
            message_body[strlen(message_body) - 1] = '\0';
            strcat(message_body, "\r\n");
            strcat(message_body, buffer);
            break;
        }
        else
        {
            strcat(message_body, buffer);
            strcat(message_body, "\r\n");
        }
    }

    // now check if the format of the from and to fields is of the form X@Y(note that there can be multiple spaces between From: and the actual username, so strip the spaces then take the username after ':')
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, from);

    char *token1 = strtok(from, " ");
    token1 = strtok(NULL, " ");
    if (strchr(token1, '@') == NULL)
    {
        printf("Error: Incorrect format for From field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    // also make sure there are no spaces in X and Y in X@Y
    if (strchr(token1, ' ') != NULL)
    {
        printf("Error: Incorrect format for From field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    memset(from, 0, sizeof(from));
    strcpy(from, buffer);

    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, to);

    char *token2 = strtok(to, " ");
    token2 = strtok(NULL, " ");
    if (strchr(token2, '@') == NULL)
    {
        printf("Error: Incorrect format for To field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    // also make sure there are no spaces in X and Y in X@Y
    if (strchr(token2, ' ') != NULL)
    {
        printf("Error: Incorrect format for To field.\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
    memset(to, 0, sizeof(to));
    strcpy(to, buffer);

    // now send the message MAIL FROM:<from_username> and check if the response is 250 OK
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, "MAIL FROM:<");
    strcat(buffer, token1);
    strcat(buffer, ">\r\n");

    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));

    // get the success message from the server, if something else received, exit
    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "250", 3) != 0)
    {
        printf("Error in MAIL FROM 250 OK...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }

    // now send the message RCPT TO:<to_username> and check if the response is 250 OK
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, "RCPT TO:<");
    strcat(buffer, token2);
    strcat(buffer, ">\r\n");

    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));

    // get the success message from the server, if something else received, exit
    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "250", 3) != 0)
    {
        if (strncmp(buffer, "550", 3) == 0)
        {
            printf("User not found...\n");
            fflush(stdout);
            send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
            return;
        }
        else
        {
            printf("Error in RCPT TO 250 OK...\n");
            fflush(stdout);
            send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
            return;
        }
    }

    // send the message DATA and check if the response is 354 Start mail input; end with <CRLF>.<CRLF>
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, "DATA\r\n");

    send(client_socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));

    // if the response is 354 Enter mail, end with "." on a line by itself<CRLF>, if something else received, exit
    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "354", 3) != 0)
    {
        printf("Error in DATA 250 OK...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }

    // now send the message to the server(it will be a multiline send)
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, from);
    strcat(buffer, "\r\n");
    strcat(buffer, to);
    strcat(buffer, "\r\n");
    strcat(buffer, subject);
    strcat(buffer, "\r\n");
    strcat(buffer, message_body);
    strcat(buffer, "\r\n");

    send(client_socket, buffer, strlen(buffer), 0);

    memset(buffer, 0, sizeof(buffer));

    // get the message 250 OK Message accepted for delivery, if something else received, exit
    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "250", 3) != 0)
    {
        printf("Error in receiving 250 OK Message accepted for delivery...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }

    // send the message QUIT and check if the response is 221 <domain name> closing connection
    memset(buffer, 0, sizeof(buffer));

    strcpy(buffer, "QUIT\r\n");

    send(client_socket, buffer, strlen(buffer), 0);

    // get the message 221 <domain name> closing connection, if something else received, exit
    memset(buffer, 0, sizeof(buffer));

    receive_buffer(client_socket, buffer);

    if (strncmp(buffer, "221", 3) != 0)
    {
        printf("Error in QUIT...\n");
        fflush(stdout);
        send(client_socket, "QUIT\r\n", strlen("QUIT\r\n"), 0);
        return;
    }
}

void receive_buffer(int socket, char *main_buffer)
{
    ssize_t bytesRead;
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    while ((bytesRead = recv(socket, buffer, sizeof(buffer), 0)) != 0)
    {
        // if <CR><LF> is not found, then continue receiving
        if (strstr(buffer, "\r\n") == NULL)
        {
            strcat(main_buffer, buffer);
            memset(buffer, 0, sizeof(buffer));
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

void get_mail(int socket, char *main_buffer, int mail_num) //mail_num is 1 indexed, mails is 0 indexed
{
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "RETR ");

    // convert the mail number to string
    char mail_id[10];
    memset(mail_id, 0, sizeof(mail_id));
    sprintf(mail_id, "%d", mail_num);
    strcat(buffer, mail_id);

    strcat(buffer, "\r\n");

    send(socket, buffer, strlen(buffer), 0);

    // multiline respone here, so keep receiving until the last line is .
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytesRead;
    char rec_buffer[MAX_BUFFER_SIZE];
    memset(rec_buffer, 0, sizeof(rec_buffer));
    int flag = 0;

    while ((bytesRead = recv(socket, rec_buffer, sizeof(rec_buffer), 0)) != 0)
    {
        if (strstr(rec_buffer, "\r\n") == NULL)
        {
            strcat(buffer, rec_buffer);
            memset(rec_buffer, 0, sizeof(rec_buffer));
            continue;
        }
        else
        {
            //if <CR><LF> is found
            while (strstr(rec_buffer, "\r\n") != NULL)
            {
                // append the part of the rec_buffer before <CR><LF> to the buffer
                char *ptr = strstr(rec_buffer, "\r\n");
                int index = ptr - rec_buffer;
                strncat(buffer, rec_buffer, index);

                // Shift the rec_buffer to the left by index+2
                memmove(rec_buffer, rec_buffer + index + 2, strlen(rec_buffer) - index - 2);
                memset(rec_buffer + strlen(rec_buffer) - index - 2, 0, index + 2);

                // now add a \n to the end of the buffer and append it to the main_buffer
                strcat(buffer, "\n");
                strcat(main_buffer, buffer);

                // in case the buffer contains .\n now, then return
                if (strncmp(buffer, ".\n", 2) == 0)
                {
                    // printf("Main buffer: %s\n", main_buffer);
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
