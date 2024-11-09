#ifndef _VPORT_H
#define _VPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "utils.h"  // Inclusion pour la fonction tap_alloc

#define BUFFER_SIZE 1518  // Taille maximale d'une trame Ethernet

// Structure représentant un vport
struct vport_t {
    int tapfd;                // Descripteur de fichier pour le périphérique TAP
    int vport_sockfd;         // Descripteur de fichier pour la socket UDP vers le VSwitch
    struct sockaddr_in vswitch_addr;  // Adresse du serveur VSwitch
    bool running;             // Indicateur de contrôle pour les threads
};

// Prototypes des fonctions de gestion des vports
void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port);
void cleanup(struct vport_t *vport);
void *forward_ether_data_to_vswitch(void *raw_vport);
void *forward_ether_data_to_tap(void *raw_vport);

#endif  // _VPORT_H
