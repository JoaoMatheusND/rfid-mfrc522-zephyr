/**
 * @file mfrc522.h
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @brief Interface de abstração do driver do leitor RFID MFRC522.
 * @version 0.1
 * @date 04-07-2026
 *
 * @copyright Copyright (c) 2026, Centro de Inovação EDGE.
 *
 */

#ifndef MFRC522_H
#define MFRC522_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup mfrc522 MFRC522.
 * @{
 */

#ifndef CONFIG_MFRC522_UID_MAX_LEN
#define CONFIG_MFRC522_UID_MAX_LEN 10
#endif

/**
 * @brief Tamanho, em bytes, de um bloco de dados MIFARE.
 */
#define MFRC522_BLOCK_SIZE 16U

/**
 * @brief Tamanho, em bytes, de uma chave de autenticação MIFARE (Key A ou Key B).
 */
#define MFRC522_KEY_SIZE 6U

/**
 * @brief Estrutura que representa o UID de um cartão PICC lido.
 */
struct mfrc522_uid {
	uint8_t bytes[CONFIG_MFRC522_UID_MAX_LEN]; /**< Bytes do UID. */
	uint8_t len;                               /**< Tamanho, em bytes, do UID. */
	uint8_t sak;                                /**< Select Acknowledge do cartão. */
};

/**
 * @brief Enumera as chaves de autenticação suportadas pelo MIFARE Classic.
 */
enum mfrc522_key_type {
	MFRC522_KEY_A = 0, /**< Autentica com a Key A do setor. */
	MFRC522_KEY_B,     /**< Autentica com a Key B do setor. */
};

/**
 * @brief Inicializa o chip MFRC522 através do transporte configurado.
 *
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_init(void);

/**
 * @brief Verifica se há um cartão PICC dentro do campo de antena.
 *
 * @return true se houver um cartão presente, false caso contrário.
 */
bool mfrc522_card_present(void);

/**
 * @brief Executa a anticolisão/seleção, lê o UID do cartão presente no campo de antena e o
 * coloca em HALT em seguida.
 *
 * @note Atalho para quem só precisa do UID. Para ler/escrever blocos, use @ref mfrc522_select
 * seguido de @ref mfrc522_authenticate, @ref mfrc522_read_block / @ref mfrc522_write_block e, ao
 * final, @ref mfrc522_halt — chamar esta função deixaria o cartão em HALT antes da autenticação.
 *
 * @param[out] uid Referência para a estrutura onde o UID lido será armazenado.
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_read_uid(struct mfrc522_uid *uid);

/**
 * @brief Executa a anticolisão/seleção do cartão presente no campo de antena, mantendo-o
 * ativo (sem HALT) para permitir autenticação e leitura/escrita de blocos.
 *
 * @param[out] uid Referência para a estrutura onde o UID/SAK lidos serão armazenados.
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_select(struct mfrc522_uid *uid);

/**
 * @brief Autentica um bloco do cartão selecionado com a Key A ou Key B do setor.
 *
 * @param block_addr Endereço absoluto do bloco (0-255, conforme o cartão).
 * @param key_type Key A ou Key B.
 * @param key Chave de 6 bytes.
 * @param uid UID retornado por @ref mfrc522_select.
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_authenticate(uint8_t block_addr, enum mfrc522_key_type key_type,
			  const uint8_t key[MFRC522_KEY_SIZE], const struct mfrc522_uid *uid);

/**
 * @brief Lê um bloco de 16 bytes do cartão. Requer autenticação prévia do bloco via
 * @ref mfrc522_authenticate.
 *
 * @param block_addr Endereço absoluto do bloco.
 * @param[out] data Buffer de 16 bytes onde os dados lidos serão armazenados.
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_read_block(uint8_t block_addr, uint8_t data[MFRC522_BLOCK_SIZE]);

/**
 * @brief Escreve um bloco de 16 bytes no cartão. Requer autenticação prévia do bloco via
 * @ref mfrc522_authenticate.
 *
 * @param block_addr Endereço absoluto do bloco.
 * @param data Buffer de 16 bytes a serem escritos.
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_write_block(uint8_t block_addr, const uint8_t data[MFRC522_BLOCK_SIZE]);

/**
 * @brief Encerra a sessão com o cartão selecionado: envia HLTA e desliga o Crypto1.
 *
 * @return true em caso de sucesso, false caso contrário.
 */
bool mfrc522_halt(void);

/**
 * @}
 */

#endif /* MFRC522_H */
