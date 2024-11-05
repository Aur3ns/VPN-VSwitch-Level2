#!/usr/bin/env python3
import socket
import sys

# Vérification des arguments en ligne de commande pour s'assurer qu'un port est fourni
if len(sys.argv) != 2:
    print("Usage: python3 vswitch.py {VSWITCH_PORT}")
    sys.exit(1)

# Tentative de conversion de l'argument en un entier pour le port du serveur
try:
    server_port = int(sys.argv[1])
    server_addr = ("0.0.0.0", server_port)  # Adresse de liaison sur toutes les interfaces réseau
except ValueError:
    print("Invalid port number.")
    sys.exit(1)

# Création du socket UDP et liaison à l'adresse et au port spécifiés
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as vserver_sock:
    vserver_sock.bind(server_addr)
    print(f"[VSwitch] Started at {server_addr[0]}:{server_addr[1]}")

    # Table MAC pour stocker les adresses MAC et leurs adresses associées (vport_addr)
    mac_table = {}

    # Boucle principale pour gérer les connexions entrantes
    while True:
        try:
            # Réception de la trame Ethernet depuis un VPort
            data, vport_addr = vserver_sock.recvfrom(1518)  # Limite à la taille d'une trame Ethernet

            # Extraction des adresses MAC de destination et de source depuis l'en-tête Ethernet
            eth_dst, eth_src = ":".join(f"{x:02x}" for x in data[:6]), ":".join(f"{x:02x}" for x in data[6:12])
            print(f"[VSwitch] src<{eth_src}> dst<{eth_dst}> from {vport_addr}")

            # Mise à jour de la table MAC avec l'adresse source et son port d'origine
            mac_table[eth_src] = vport_addr

            # Transfert de la trame en fonction de l'adresse de destination
            if eth_dst in mac_table:
                # Si l'adresse MAC de destination est dans la table MAC, envoi unicast au port associé
                vserver_sock.sendto(data, mac_table[eth_dst])
            elif eth_dst == "ff:ff:ff:ff:ff:ff":
                # Si l'adresse MAC de destination est une adresse de broadcast, envoyer à tous sauf à la source
                for dst in mac_table.values():
                    if dst != vport_addr:  # Évite de renvoyer au port source
                        vserver_sock.sendto(data, dst)
            else:
                # Sinon, la trame est ignorée (MAC de destination inconnue)
                print("Discarded unknown destination")
        
        # Gestion des exceptions pour éviter l'arrêt du programme en cas d'erreur
        except Exception as e:
            print(f"Error: {e}")
