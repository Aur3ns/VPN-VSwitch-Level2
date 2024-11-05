#include "utils.h"
#include <pthread.h>

struct vport_t {
    int tapfd;
    int vport_sockfd;
    struct sockaddr_in vswitch_addr;
};

int tap_alloc(char *dev) {
    struct ifreq ifr;
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) return fd;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        close(fd);
        return -errno;
    }
    strncpy(dev, ifr.ifr_name, IFNAMSIZ);
    return fd;
}

// Reste des fonctions `vport_init`, `forward_ether_data_to_vswitch`, etc.
void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port);
void *forward_ether_data_to_vswitch(void *raw_vport);
void *forward_ether_data_to_tap(void *raw_vport);
void cleanup(struct vport_t *vport);

int main(int argc, char const *argv[])
{
  if (argc != 3)
  {
    ERROR_PRINT_THEN_EXIT("Usage: vport {server_ip} {server_port}\n");
  }
  const char *server_ip_str = argv[1];
  int server_port = atoi(argv[2]);

  struct vport_t vport;
  vport_init(&vport, server_ip_str, server_port);

  pthread_t up_forwarder, down_forwarder;

  if (pthread_create(&up_forwarder, NULL, forward_ether_data_to_vswitch, &vport) != 0)
  {
    cleanup(&vport);
    ERROR_PRINT_THEN_EXIT("Failed to create up_forwarder thread: %s\n", strerror(errno));
  }

  if (pthread_create(&down_forwarder, NULL, forward_ether_data_to_tap, &vport) != 0)
  {
    pthread_cancel(up_forwarder);
    cleanup(&vport);
    ERROR_PRINT_THEN_EXIT("Failed to create down_forwarder thread: %s\n", strerror(errno));
  }

  if (pthread_join(up_forwarder, NULL) != 0 || pthread_join(down_forwarder, NULL) != 0)
  {
    cleanup(&vport);
    ERROR_PRINT_THEN_EXIT("Thread join failed: %s\n", strerror(errno));
  }

  cleanup(&vport);
  return 0;
}

void vport_init(struct vport_t *vport, const char *server_ip_str, int server_port)
{
  char ifname[IFNAMSIZ] = "tapyuan";
  int tapfd = tap_alloc(ifname);
  if (tapfd < 0)
  {
    ERROR_PRINT_THEN_EXIT("Failed to allocate TAP device: %s\n", strerror(errno));
  }

  int vport_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (vport_sockfd < 0)
  {
    close(tapfd);
    ERROR_PRINT_THEN_EXIT("Failed to create socket: %s\n", strerror(errno));
  }

  struct sockaddr_in vswitch_addr;
  memset(&vswitch_addr, 0, sizeof(vswitch_addr));
  vswitch_addr.sin_family = AF_INET;
  vswitch_addr.sin_port = htons(server_port);
  if (inet_pton(AF_INET, server_ip_str, &vswitch_addr.sin_addr) != 1)
  {
    close(tapfd);
    close(vport_sockfd);
    ERROR_PRINT_THEN_EXIT("Invalid IP address format: %s\n", strerror(errno));
  }

  vport->tapfd = tapfd;
  vport->vport_sockfd = vport_sockfd;
  vport->vswitch_addr = vswitch_addr;

  printf("[VPort] TAP device name: %s, VSwitch: %s:%d\n", ifname, server_ip_str, server_port);
}

void cleanup(struct vport_t *vport)
{
  if (vport->tapfd >= 0)
    close(vport->tapfd);
  if (vport->vport_sockfd >= 0)
    close(vport->vport_sockfd);
}

void *forward_ether_data_to_vswitch(void *raw_vport)
{
  struct vport_t *vport = (struct vport_t *)raw_vport;
  char ether_data[BUFFER_SIZE];
  while (true)
  {
    int ether_datasz = read(vport->tapfd, ether_data, sizeof(ether_data));
    if (ether_datasz < 0)
    {
      perror("Failed to read from TAP device");
      break;
    }
    assert(ether_datasz >= 14);

    ssize_t sendsz = sendto(vport->vport_sockfd, ether_data, ether_datasz, 0,
                            (struct sockaddr *)&vport->vswitch_addr, sizeof(vport->vswitch_addr));
    if (sendsz != ether_datasz)
    {
      fprintf(stderr, "sendto size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
    }
  }
  return NULL;
}

void *forward_ether_data_to_tap(void *raw_vport)
{
  struct vport_t *vport = (struct vport_t *)raw_vport;
  char ether_data[BUFFER_SIZE];
  while (true)
  {
    socklen_t vswitch_addr_len = sizeof(vport->vswitch_addr);
    int ether_datasz = recvfrom(vport->vport_sockfd, ether_data, sizeof(ether_data), 0,
                                (struct sockaddr *)&vport->vswitch_addr, &vswitch_addr_len);
    if (ether_datasz < 0)
    {
      perror("Failed to receive from VSwitch");
      break;
    }
    assert(ether_datasz >= 14);

    ssize_t sendsz = write(vport->tapfd, ether_data, ether_datasz);
    if (sendsz != ether_datasz)
    {
      fprintf(stderr, "write size mismatch: ether_datasz=%d, sendsz=%zd\n", ether_datasz, sendsz);
    }
  }
  return NULL;
}
