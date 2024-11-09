#include "utils.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 1518  // Taille maximale d'une trame Ethernet

struct vport_t {
    int tapfd;                // Descripteur de fichier pour le périphérique TAP
    int vport_sockfd;         // Descripteur de fichier pour la socket UDP vers le VSwitch
    struct sockaddr_in vswitch_addr;  // Adresse du serveur VSwitch
    bool running;             // Indicateur de contrôle pour les threads
};

// Fonction pour allouer et configurer un périphérique TAP
int tap_alloc(char *dev) {
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);  // Ouvrir le périphérique TAP
    if (fd < 0) return fd;  // Retourner une erreur si échec de l'ouverture

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;  // Configurer comme périphérique TAP sans info paquet
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // Assurer une terminaison nulle

    // Définir les paramètres du périphérique TAP
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        close(fd);
        return -errno;
    }
    strncpy(dev, ifr.ifr_name, IFNAMSIZ - 1);
    dev[IFNAMSIZ - 1] = '\0';
    return fd;
}

// Fonction de nettoyage des ressources (fermeture des descripteurs de fichiers)
void cleanup(struct vport_t *vport) {
    if (vport->tapfd >= 0) close(vport->tapfd);
    if (vport->vport_sockfd >= 0) close(vport->vport_sockfd);
}

// Initialisation du VPort et connexion au VSwitch
void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port) {
    char ifname[IFNAMSIZ] = "tapyuan";  // Nom par défaut du périphérique TAP
    int tapfd = tap_alloc(ifname);  // Allouer le périphérique TAP
    if (tapfd < 0) {
        ERROR_PRINT_THEN_EXIT("Failed to allocate TAP device: %s\n", strerror(errno));
    }

    int vport_sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // Créer une socket UDP
    if (vport_sockfd < 0) {
        close(tapfd);
        ERROR_PRINT_THEN_EXIT("Failed to create socket: %s\n", strerror(errno));
    }

    // Configurer l'adresse du serveur (VSwitch)
    struct sockaddr_in vswitch_addr;
    memset(&vswitch_addr, 0, sizeof(vswitch_addr));
    vswitch_addr.sin_family = AF_INET;
    vswitch_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip_str, &vswitch_addr.sin_addr) != 1) {
        close(tapfd);
        close(vport_sockfd);
        ERROR_PRINT_THEN_EXIT("Invalid IP address format: %s\n", strerror(errno));
    }

    // Initialiser les champs de la structure VPort
    vport->tapfd = tapfd;
    vport->vport_sockfd = vport_sockfd;
    vport->vswitch_addr = vswitch_addr;
    vport->running = true;

    printf("[VPort] TAP device name: %s, VSwitch: %s:%d\n", ifname, server_ip_str, server_port);
}

// Thread pour transférer les données Ethernet du TAP au VSwitch
void *forward_ether_data_to_vswitch(void *raw_vport) {
    struct vport_t *vport = (struct vport_t *)raw_vport;
    char ether_data[BUFFER_SIZE];
    while (vport->running) {
        // Lire les données du périphérique TAP
        int ether_datasz = read(vport->tapfd, ether_data, sizeof(ether_data));
        if (ether_datasz < 0) {
            perror("Failed to read from TAP device");
            break;
        }

        // Envoyer les données au VSwitch via la socket UDP
        ssize_t sendsz = sendto(vport->vport_sockfd, ether_data, ether_datasz, 0,
                                (struct sockaddr *)&vport->vswitch_addr, sizeof(vport->vswitch_addr));
        if (sendsz != ether_datasz) {
            fprintf(stderr, "sendto size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
        }
    }
    return NULL;
}

// Thread pour transférer les données Ethernet du VSwitch au TAP
void *forward_ether_data_to_tap(void *raw_vport) {
    struct vport_t *vport = (struct vport_t *)raw_vport;
    char ether_data[BUFFER_SIZE];
    while (vport->running) {
        socklen_t vswitch_addr_len = sizeof(vport->vswitch_addr);
        // Recevoir les données du VSwitch via la socket UDP
        int ether_datasz = recvfrom(vport->vport_sockfd, ether_data, sizeof(ether_data), 0,
                                    (struct sockaddr *)&vport->vswitch_addr, &vswitch_addr_len);
        if (ether_datasz < 0) {
            perror("Failed to receive from VSwitch");
            break;
        }

        // Écrire les données dans le périphérique TAP
        ssize_t sendsz = write(vport->tapfd, ether_data, ether_datasz);
        if (sendsz != ether_datasz) {
            fprintf(stderr, "write size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
        }
    }
    return NULL;
}

// Fonction principale pour initialiser et exécuter le VPort
int main(int argc, char const *argv[]) {
    // Vérifier les arguments de la ligne de commande
    if (argc != 3) {
        ERROR_PRINT_THEN_EXIT("Usage: vport {server_ip} {server_port}\n");
    }
    const char *server_ip_str = argv[1];
    int server_port = atoi(argv[2]);

    // Initialiser la structure VPort et se connecter au VSwitch
    struct vport_t vport;
    vport_init(&vport, server_ip_str, server_port);

    pthread_t up_forwarder, down_forwarder;

    // Créer un thread pour envoyer les données du TAP au VSwitch
    if (pthread_create(&up_forwarder, NULL, forward_ether_data_to_vswitch, &vport) != 0) {
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Failed to create up_forwarder thread: %s\n", strerror(errno));
    }

    // Créer un thread pour recevoir les données du VSwitch et les transférer au TAP
    if (pthread_create(&down_forwarder, NULL, forward_ether_data_to_tap, &vport) != 0) {
        pthread_cancel(up_forwarder);
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Failed to create down_forwarder thread: %s\n", strerror(errno));
    }

    // Attendre la fin des threads
    if (pthread_join(up_forwarder, NULL) != 0 || pthread_join(down_forwarder, NULL) != 0) {
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Thread join failed: %s\n", strerror(errno));
    }

    // Nettoyer les ressources
    cleanup(&vport);
    return 0;
}
