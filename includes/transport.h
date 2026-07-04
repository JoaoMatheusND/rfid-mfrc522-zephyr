/**
 * @file transport.h
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @brief Interface de abstração dos protocolos usados pelo MFRC522.
 * @version 0.1
 * @date 04-07-2026
 *
 * @copyright Copyright (c) 2026, Centro de Inovação EDGE.
 *
 */

#ifndef MFRC522_TRANSPORT_H
#define MFRC522_TRANSPORT_H

#include <stdint.h>

/**
 * @brief Inicializa o barramento de transporte e os pinos de controle do MFRC522.
 */
void mfrc522_transport_init(void);

/**
 * @brief Lê um registrador do MFRC522.
 *
 * @param addr Endereço do registrador.
 * @param[out] val Valor lido.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
int mfrc522_transport_reg_read(uint8_t addr, uint8_t *val);

/**
 * @brief Escreve em um registrador do MFRC522.
 *
 * @param addr Endereço do registrador.
 * @param val Valor a ser escrito.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
int mfrc522_transport_reg_write(uint8_t addr, uint8_t val);

#endif /* MFRC522_TRANSPORT_H */
