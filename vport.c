#include "utils.h"
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>

#define BUFFER_SIZE 1518  // Maximum Ethernet frame size

struct vport_t {
    int tapfd;                // File descriptor for TAP device
    int vport_sockfd;         // UDP socket file descriptor to connect to VSwitch
    struct sockaddr_in vswitch_addr;  // Address of the VSwitch server
    bool running;             // Control flag to keep threads running
};

// Allocate and configure TAP device
int tap_alloc(char *dev) {
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);  // Open TAP device
    if (fd < 0) return fd;  // Return error if unable to open device

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;  // Configure as TAP device without packet info
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // Ensure null-terminated string

    // Set TAP device parameters
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        close(fd);
        return -errno;
    }
    strncpy(dev, ifr.ifr_name, IFNAMSIZ - 1);
    dev[IFNAMSIZ - 1] = '\0';
    return fd;
}

// Clean up resources (close TAP and socket descriptors)
void cleanup(struct vport_t *vport) {
    if (vport->tapfd >= 0) close(vport->tapfd);
    if (vport->vport_sockfd >= 0) close(vport->vport_sockfd);
}

// Initialize VPort and connect to VSwitch
void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port) {
    char ifname[IFNAMSIZ] = "tapyuan";  // Default TAP device name
    int tapfd = tap_alloc(ifname);  // Allocate TAP device
    if (tapfd < 0) {
        ERROR_PRINT_THEN_EXIT("Failed to allocate TAP device: %s\n", strerror(errno));
    }

    int vport_sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // Create UDP socket
    if (vport_sockfd < 0) {
        close(tapfd);
        ERROR_PRINT_THEN_EXIT("Failed to create socket: %s\n", strerror(errno));
    }

    // Configure server (VSwitch) address
    struct sockaddr_in vswitch_addr;
    memset(&vswitch_addr, 0, sizeof(vswitch_addr));
    vswitch_addr.sin_family = AF_INET;
    vswitch_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip_str, &vswitch_addr.sin_addr) != 1) {
        close(tapfd);
        close(vport_sockfd);
        ERROR_PRINT_THEN_EXIT("Invalid IP address format: %s\n", strerror(errno));
    }

    // Initialize VPort structure fields
    vport->tapfd = tapfd;
    vport->vport_sockfd = vport_sockfd;
    vport->vswitch_addr = vswitch_addr;
    vport->running = true;

    printf("[VPort] TAP device name: %s, VSwitch: %s:%d\n", ifname, server_ip_str, server_port);
}

// Thread to forward Ethernet data from TAP to VSwitch
void *forward_ether_data_to_vswitch(void *raw_vport) {
    struct vport_t *vport = (struct vport_t *)raw_vport;
    char ether_data[BUFFER_SIZE];
    while (vport->running) {
        // Read data from TAP device
        int ether_datasz = read(vport->tapfd, ether_data, sizeof(ether_data));
        if (ether_datasz < 0) {
            perror("Failed to read from TAP device");
            break;
        }

        // Send data to VSwitch via UDP socket
        ssize_t sendsz = sendto(vport->vport_sockfd, ether_data, ether_datasz, 0,
                                (struct sockaddr *)&vport->vswitch_addr, sizeof(vport->vswitch_addr));
        if (sendsz != ether_datasz) {
            fprintf(stderr, "sendto size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
        }
    }
    return NULL;
}

// Thread to forward Ethernet data from VSwitch to TAP
void *forward_ether_data_to_tap(void *raw_vport) {
    struct vport_t *vport = (struct vport_t *)raw_vport;
    char ether_data[BUFFER_SIZE];
    while (vport->running) {
        socklen_t vswitch_addr_len = sizeof(vport->vswitch_addr);
        // Receive data from VSwitch via UDP socket
        int ether_datasz = recvfrom(vport->vport_sockfd, ether_data, sizeof(ether_data), 0,
                                    (struct sockaddr *)&vport->vswitch_addr, &vswitch_addr_len);
        if (ether_datasz < 0) {
            perror("Failed to receive from VSwitch");
            break;
        }

        // Write data to TAP device
        ssize_t sendsz = write(vport->tapfd, ether_data, ether_datasz);
        if (sendsz != ether_datasz) {
            fprintf(stderr, "write size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
        }
    }
    return NULL;
}

// Main function to initialize and run VPort
int main(int argc, char const *argv[]) {
    // Check command line arguments
    if (argc != 3) {
        ERROR_PRINT_THEN_EXIT("Usage: vport {server_ip} {server_port}\n");
    }
    const char *server_ip_str = argv[1];
    int server_port = atoi(argv[2]);

    // Initialize VPort structure and connect to VSwitch
    struct vport_t vport;
    vport_init(&vport, server_ip_str, server_port);

    pthread_t up_forwarder, down_forwarder;

    // Create thread to send data from TAP to VSwitch
    if (pthread_create(&up_forwarder, NULL, forward_ether_data_to_vswitch, &vport) != 0) {
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Failed to create up_forwarder thread: %s\n", strerror(errno));
    }

    // Create thread to receive data from VSwitch and forward to TAP
    if (pthread_create(&down_forwarder, NULL, forward_ether_data_to_tap, &vport) != 0) {
        pthread_cancel(up_forwarder);
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Failed to create down_forwarder thread: %s\n", strerror(errno));
    }

    // Wait for threads to complete
    if (pthread_join(up_forwarder, NULL) != 0 || pthread_join(down_forwarder, NULL) != 0) {
        cleanup(&vport);
        ERROR_PRINT_THEN_EXIT("Thread join failed: %s\n", strerror(errno));
    }

    // Clean up resources
    cleanup(&vport);
    return 0;
}
