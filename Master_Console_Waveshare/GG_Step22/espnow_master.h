#pragma once
#include "espnow_protocol.h"

// Init ESP-NOW (Waveshare = master)
void espnow_master_init();

// Envoyer la commande des relais au slave
//  bitmask    : etat des 8 relais (bit 0=Salon, bit1=Cuisine, ..., bit7=Divers)
//  chauffe_eau: 0=OFF, 1=ON → MMBT2222A K1 (GPIO25 slave)
bool espnow_send_relays(uint8_t bitmask, uint8_t chauffe_eau);

// Envoyer un ping (keepalive)
void espnow_send_ping();

// Acces aux dernieres donnees recues du slave
extern StatusMsg statusFromSlave;
extern bool      statusValid;
extern uint32_t  statusLastUpdateMs;
extern bool      slaveOnline;
