//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//
//                     2º de grado de Ingeniería Informática
//
//              This class processes an FTP transactions.
//
//****************************************************************************

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h> // para chdir() , cambiar de directorio

#include <sys/stat.h>
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"

//
int define_socket(int port)
{
    // Include the code for defining the socket.
    struct sockaddr_in sin;
    int s;

    s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
        errexit("\nNo se pudo crear el socket: %s\n", strerror(errno));

    memset(&sin, 0, sizeof(sin)); //Pone  sin a 0.
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    auto bind_ = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (bind_ < 0)
        errexit("\nNo se pudo hacer el bind con el puerto: %s\n", strerror(errno));

    if (listen(s, 5) < 0)
        errexit("\nFallo en el listen: %s\n", strerror(errno));

    return s;
}
//

ClientConnection::ClientConnection(int s)
{
    int sock = (int)(s);

    char buffer[MAX_BUFF];

    control_socket = s;
    // Check the Linux man pages to know what fdopen does.
    fd = fdopen(s, "a+");
    if (fd == NULL)
    {
        std::cout << "Connection closed" << std::endl;

        fclose(fd);
        close(control_socket);
        ok = false;
        return;
    }

    ok = true;
    data_socket = -1;
};

ClientConnection::~ClientConnection()
{
    fclose(fd);
    close(control_socket);
}

int connect_TCP(uint32_t address, uint16_t port)
{
    // Implement your code to define a socket here
    struct sockaddr_in sin;
    struct hostent *hent;
    int s;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = address;
    sin.sin_port = htons(port);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        errexit("\nNo se pudo crear el socket: %s\n", strerror(errno));

    auto connect_ = connect(s, (struct sockaddr *)&sin, sizeof(sin));
    if (connect_ < 0)
        errexit("\nNo se pudo conectar con %d: %s\n", address, strerror(errno));

    return s; // You must return the socket descriptor.
}

void ClientConnection::stop()
{
    close(data_socket);
    close(control_socket);
    parar = true;
}

#define COMMAND(cmd) strcmp(command, cmd) == 0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests()
{
    if (!ok)
    {
        return;
    }

    fprintf(fd, "220 Service ready\n");

    while (!parar)
    {

        fscanf(fd, "%s", command);
        if (COMMAND("USER"))
        {
            fscanf(fd, "%s", arg);
            fprintf(fd, "331 User name ok, need password\n");
        }
        else if (COMMAND("PWD")) //Print Working directory.
        {
            auto path = get_current_dir_name();
            fprintf(fd, "257 %s current working directory\n", path);
        }
        else if (COMMAND("PASS"))
        {
            fscanf(fd, "%s", arg);
            //if (!strcmp(arg, "naranjito"))
            fprintf(fd, "230 User logged in, proceed\n");
            //else
            //fprintf(fd, "530 Not logged in.\n");
        }
        else if (COMMAND("PORT"))
        {
            /*The argument is a HOST-PORT specification for the data port
            to be used in data connection. The fields are separated by commas.
            A port command would be:
            PORT h1,h2,h3,h4,p1,p2
            where h1 is the high order 8 bits of the internet host
            address. */
            int ip_addr[4];
            int port_[2];
            fscanf(fd, "%d,%d,%d,%d,%d,%d", &ip_addr[0], &ip_addr[1], &ip_addr[2], &ip_addr[3], &port_[0], &port_[1]);

            uint32_t address = ip_addr[0] | ip_addr[1] << 8 | ip_addr[2] << 16 | ip_addr[3] << 24;
            uint16_t port = port_[0] << 8 | port_[1];

            data_socket = connect_TCP(address, port);

            fprintf(fd, "200 OK.\n");
        }
        else if (COMMAND("PASV")) //Pasive
        {
            /*This command requests the server-DTP to "listen" on a data
            port (which is not its default data port) and to wait for a
            connection rather than initiate one upon receipt of a
            transfer command. The response to this command includes the
            host and port address this server is listening on.*/
            //printf("ERROR0");

            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            int s = define_socket(0);

            getsockname(s, (struct sockaddr *)&sin, &len);

            uint16_t port = sin.sin_port;
            int p1 = port & 0xFF;
            int p2 = port >> 8;

            fprintf(fd, "227 Entering Passive Mode (127,0,0,1,%d,%d).\n", p1, p2);
            fflush(fd);

            len = sizeof(sin);
            data_socket = accept(s, (struct sockaddr *)&sin, &len);

        }
        else if (COMMAND("CWD")) //Change Working Directory
        {
            /*This command allows the user to work with a different
            directory or dataset for file storage or retrieval without
            altering his login or accounting information.*/

            fscanf(fd, "%s", arg);
            int cwd = chdir(arg);
            if (cwd < 0)
            {
                fprintf(fd, "501 Syntax error in parameters or arguments.");
                errexit("\nNo se pudo cambiar el directorio actual de trabajo: %s\n", strerror(errno));
            }
            else
            {
                fprintf(fd, "250 Requested file action okay, completed.\n");
            }
        }
        else if (COMMAND("STOR")) //Store
        {
            /*This command causes the server-DTP to accept the data
            transferred via the data connection and to store the data as a
            file at the server site.*/

            fscanf(fd, "%s", arg);
            FILE *file = fopen(arg, "wb");
            if (!file)
            {
                fprintf(fd, "450 Requested file action not taken. File unavailable.\n");
                close(data_socket);
            }
            else
            {
                fprintf(fd, "150 File status okay; about to open data connection.\n");
                fflush(fd);
                char buffer[MAX_BUFF];
                size_t receive_read;

                do
                {
                    //receive_read = fread(buffer, 1, MAX_BUFF, file);
                    receive_read = recv(data_socket, buffer, MAX_BUFF, 0);

                    fwrite(buffer, 1, receive_read, file);
                    //write(data_socket, buffer, receive_read);

                } while (receive_read == MAX_BUFF);

                fprintf(fd, "226  Closing data connection.\n");
                fclose(file);
                close(data_socket);
            }
        }
        else if (COMMAND("SYST"))
        {
            fprintf(fd, "215 UNIX Type: L8.\n");
        }
        else if (COMMAND("TYPE")) //Representation type
        {
            /*The argument specifies the representation type as described
            in the Section on Data Representation and Storage.*/

            fscanf(fd, "%s", arg);
            fprintf(fd, "200 OK.\n");
            fflush(fd);
        }
        else if (COMMAND("RETR")) //Retrieve
        {
            /*This command causes the server-DTP to transfer a copy of the
            file, specified in the pathname, to the server- or user-DTP
            at the other end of the data connection.*/

            fscanf(fd, "%s", arg);
            FILE *file = fopen(arg, "rb");
            if (!file)
            {
                fprintf(fd, "450 Requested file action not taken.\nFile unavailable.\n");
                close(data_socket);
            }
            else
            {
                fprintf(fd, "150 File status okay; about to open data connection.\n");
                fflush(fd);
                char buffer[MAX_BUFF];

                size_t receive_read = 0;

                do
                {
                    receive_read = fread(buffer, 1, MAX_BUFF, file);

                    /*auto check_error =*/send(data_socket, buffer, receive_read, 0);
                    //if(check_error < 0)

                } while (receive_read == MAX_BUFF);

                fprintf(fd, "226  Closing data connection.\n");
                fclose(file);
                close(data_socket);
            }
        }
        else if (COMMAND("QUIT"))
        {
            fprintf(fd, "221 Service closing control connection.\nLogged out if appropiate.\n");
            // parar = true;
        }
        else if (COMMAND("LIST"))
        {
            /*This command causes a list to be sent from the server to the
            passive DTP. If the pathname specifies a directory or other
            group of files, the server should transfer a list of files
            in the specified directory. If the pathname specifies a
            /*file then the server should send current information on the file*/

            // fscanf(fd, "%s", arg);
            fprintf(fd, "125 Data connection already open; transfer starting.\n");
            fflush(fd);


            // DIR *dir;
            // if(arg != "."){
            //     dir = opendir(arg);
            // }
            // else
            // {
            //     dir = opendir(get_current_dir_name());
            // }
            DIR *dir = opendir(get_current_dir_name());
            char *file_name;
            struct dirent *dir_entry;

            size_t sz;
            char buffer[MAX_BUFF];

            if (dir == NULL)
            {
                fprintf(fd, "450 Requested file action not taken. File unavailable.\n");
                close(data_socket);
            }
            else
            {
                while ((dir_entry = readdir(dir)) != NULL)
                {
                    sz = sprintf(buffer, "%s\n", dir_entry->d_name); // introduce en el buffer lo que ha leído 
                    send(data_socket, buffer, sz, 0);
                }
            }

            fprintf(fd, "250 List completed successfully.\n");
            fflush(fd);
            closedir(dir);
            close(data_socket);
        }
        else
        {
            fprintf(fd, "502 Command not implemented.\n");
            fflush(fd);
            printf("Comando : %s %s\n", command, arg);
            printf("Error interno del servidor\n");
        }
    }

    fclose(fd);

    return;
};
