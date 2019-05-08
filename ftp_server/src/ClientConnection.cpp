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
#include <unistd.h>

#include <sys/stat.h>
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"

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
        else if (COMMAND("PWD"))
        {
            auto path = get_current_dir_name();
            fprintf(fd, "257 %s current working directory\n", path);
        }
        else if (COMMAND("PASS"))
        {
            fscanf(fd, "%s", arg);
            if (!strcmp(arg, "naranjito"))
                fprintf(fd, "230 User logged in, proceed\n");
            else
                fprintf(fd, "530 Not logged in.\n");
        }
        else if (COMMAND("PORT"))
        {
            // uint32_t ip_addr;
            // uint16_t port;
            // fscanf(fd, "%d,%d,%d,%d,%d,%d", ip_addr, ip_addr << 8, ip_addr << 16, ip_addr << 24, port << 8, port);

            // data_socket = connect_TCP(ip_addr, port);

            int ip[4];
            int puertos[2];
            fscanf(fd, "%d,%d,%d,%d,%d,%d", &ip[0], &ip[1], &ip[2], &ip[3], &puertos[0], &puertos[1]);

            uint32_t ip_addr = ip[0] | ip[1] << 8 | ip[2] << 16 | ip[3]<<24;
            uint16_t port_v = puertos[0] << 8 | puertos[1];

            data_socket = connect_TCP(ip_addr, port_v);

            fprintf(fd, "200 OK.\n");
        }
        else if (COMMAND("PASV"))
        {
            fprintf(fd, "227 Entering Passive Mode.\n");
        }
        else if (COMMAND("CWD"))
        {
            fscanf(fd, "%s", arg);
            fprintf(fd, "250 Requested file action okay, completed.\n");
        }
        else if (COMMAND("STOR"))
        {
            //fprintf(fd, "125 Data connection already open; transfer starting.\n");
            //fprintf(fd, "150 File status okay; about to open data connection.\n");
        }
        else if (COMMAND("SYST"))
        {
            fprintf(fd, "215 UNIX Type: L8.\n");
        }
        else if (COMMAND("TYPE"))
        {
            fscanf(fd, "%s", arg);
            fprintf(fd, "200 OK.\n");
        }
        else if (COMMAND("RETR"))
        {
            fscanf(fd, "%s", arg);
            fprintf(fd, "150 File status okay; about to open data connection.\n");
        }
        else if (COMMAND("QUIT"))
        {
            fprintf(fd, "221 Service closing control connection.\nLogged out if appropiate.\n");
        }
        else if (COMMAND("LIST"))
        {
            fscanf(fd, "%s", arg);
            fprintf(fd, "125 Data connection already open; transfer starting.\n");
        }
        //else if (COMMAND("get README"))
        //{
        //}
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
