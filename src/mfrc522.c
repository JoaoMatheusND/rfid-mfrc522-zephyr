/**
 * @file mfrc522.c
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @brief Implementa o driver do leitor RFID MFRC522.
 * @version 0.1
 * @date 04-07-2026
 *
 * @copyright Copyright (c) 2026, Centro de Inovação EDGE.
 *
 */

#include "mfrc522.h"
#include "mfrc522_transport.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mfrc522, CONFIG_MFRC522_LOG_LEVEL);

#define REG_STATUS2      0x08U
#define REG_COMMAND      0x01U
#define REG_COM_IRQ      0x04U
#define REG_DIV_IRQ      0x05U
#define REG_ERROR        0x06U
#define REG_FIFO_DATA    0x09U
#define REG_FIFO_LEVEL   0x0AU
#define REG_BIT_FRAMING  0x0DU
#define REG_MODE         0x11U
#define REG_TX_CONTROL   0x14U
#define REG_TX_ASK       0x15U
#define REG_T_MODE       0x2AU
#define REG_T_PRESCALER  0x2BU
#define REG_T_RELOAD_H   0x2CU
#define REG_T_RELOAD_L   0x2DU
#define REG_CRC_RESULT_M 0x21U
#define REG_CRC_RESULT_L 0x22U
#define REG_VERSION      0x37U

#define CMD_IDLE       0x00U
#define CMD_CALC_CRC   0x03U
#define CMD_TRANSCEIVE 0x0CU
#define CMD_MF_AUTHENT 0x0EU
#define CMD_SOFT_RESET 0x0FU

#define PICC_WUPA       0x52U
#define PICC_HLTA       0x50U
#define PICC_CT         0x88U /**< Byte de marcação de cascata. */
#define PICC_CL1        0x93U
#define PICC_CL2        0x95U
#define PICC_CL3        0x97U
#define PICC_SELECT_NVB 0x70U

#define PICC_MF_AUTH_KEY_A 0x60U
#define PICC_MF_AUTH_KEY_B 0x61U
#define PICC_MF_READ       0x30U
#define PICC_MF_WRITE      0xA0U

/**
 * @brief Bit do Status2Reg que indica que o Crypto1 está ativo (sessão autenticada).
 */
#define STATUS2_CRYPTO1_ON 0x08U

/**
 * @brief Nibble de ACK esperado do PICC após CMD_MF_WRITE.
 */
#define PICC_MF_ACK 0x0AU

#define TRANSCEIVE_TIMEOUT_MS CONFIG_MFRC522_TRANSCEIVE_TIMEOUT_MS

/**
 * @brief Estrutura de controle do driver.
 */
static struct mfrc522_control {
	bool initialized; /**< Indica se o chip já foi inicializado com sucesso. */
} self = {
	.initialized = false,
};

static inline int reg_write(uint8_t addr, uint8_t val)
{
	return transport_reg_write(addr, val);
}

static inline int reg_read(uint8_t addr, uint8_t *val)
{
	return transport_reg_read(addr, val);
}

/**
 * @brief Envia tx_data, recebe em rx_data via CMD_TRANSCEIVE.
 *
 * @param tx_data Dados a serem enviados.
 * @param tx_len Tamanho de tx_data.
 * @param[out] rx_data Buffer para os dados recebidos.
 * @param rx_max Tamanho máximo de rx_data.
 * @param last_bits Número de bits válidos no último byte de tx_data (0 = byte completo, 7 =
 * REQA/WUPA).
 * @return Número de bytes recebidos, ou código de erro negativo.
 */
static int transceive(const uint8_t *tx_data, uint8_t tx_len, uint8_t *rx_data, uint8_t rx_max,
		      uint8_t last_bits)
{
	uint8_t irq, err, n;
	int ret;
	int timeout = TRANSCEIVE_TIMEOUT_MS;

	ret = reg_write(REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_COM_IRQ, 0x7FU); /* Limpa todas as flags de IRQ. */
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_FIFO_LEVEL, 0x80U); /* Esvazia o FIFO. */
	if (ret != 0) {
		return ret;
	}

	for (uint8_t i = 0; i < tx_len; i++) {
		ret = reg_write(REG_FIFO_DATA, tx_data[i]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = reg_write(REG_BIT_FRAMING, last_bits & 0x07U);
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_COMMAND, CMD_TRANSCEIVE);
	if (ret != 0) {
		return ret;
	}
	/* StartSend dispara a transmissão. */
	ret = reg_write(REG_BIT_FRAMING, (last_bits & 0x07U) | 0x80U);
	if (ret != 0) {
		return ret;
	}

	/* Poll: RxIRq(5) | IdleIRq(4) | TimerIRq(0). */
	do {
		ret = reg_read(REG_COM_IRQ, &irq);
		if (ret != 0) {
			return ret;
		}
		k_msleep(1);
		timeout--;
	} while (!(irq & 0x31U) && timeout > 0);

	/* Para o comando Transceive antes de mexer no FIFO. */
	reg_write(REG_COMMAND, CMD_IDLE);

	if (timeout == 0 || (irq & 0x01U)) {
		LOG_DBG("transceive: timed out");
		return -ETIMEDOUT;
	}

	ret = reg_read(REG_ERROR, &err);
	if (ret != 0) {
		return ret;
	}
	if (err & 0x1FU) { /* BufferOvfl | CollErr | CRCErr | ParityErr | ProtocolErr. */
		LOG_DBG("transceive: error detected");
		return -EIO;
	}

	ret = reg_read(REG_FIFO_LEVEL, &n);
	if (ret != 0) {
		return ret;
	}

	uint8_t to_read = (n < rx_max) ? n : rx_max;

	for (uint8_t i = 0; i < to_read; i++) {
		ret = reg_read(REG_FIFO_DATA, &rx_data[i]);
		if (ret != 0) {
			return ret;
		}
	}

	return (int)to_read;
}

/**
 * @brief Calcula o CRC de um bloco de dados através do coprocessador interno do MFRC522.
 *
 * @param data Dados de entrada.
 * @param len Tamanho de data.
 * @param[out] crc_l Byte menos significativo do CRC.
 * @param[out] crc_h Byte mais significativo do CRC.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int calc_crc(const uint8_t *data, uint8_t len, uint8_t *crc_l, uint8_t *crc_h)
{
	uint8_t irq;
	int ret;
	int timeout = TRANSCEIVE_TIMEOUT_MS;

	ret = reg_write(REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_DIV_IRQ, 0x04U); /* Limpa CRCIRq. */
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_FIFO_LEVEL, 0x80U); /* Esvazia o FIFO. */
	if (ret != 0) {
		return ret;
	}

	for (uint8_t i = 0; i < len; i++) {
		ret = reg_write(REG_FIFO_DATA, data[i]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = reg_write(REG_COMMAND, CMD_CALC_CRC);
	if (ret != 0) {
		return ret;
	}

	do {
		ret = reg_read(REG_DIV_IRQ, &irq);
		if (ret != 0) {
			return ret;
		}
		k_msleep(1);
		timeout--;
	} while (!(irq & 0x04U) && timeout > 0);

	if (timeout == 0) {
		LOG_DBG("calc_crc: timed out");
		return -ETIMEDOUT;
	}

	ret = reg_write(REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		return ret;
	}
	ret = reg_read(REG_CRC_RESULT_L, crc_l);
	if (ret != 0) {
		return ret;
	}
	return reg_read(REG_CRC_RESULT_M, crc_h);
}

/**
 * @brief Verifica a presença de um cartão através de um WUPA, que detecta cartões tanto em
 * IDLE quanto em HALT.
 */
static bool chip_card_present(void)
{
	uint8_t rx[2];
	uint8_t req = PICC_WUPA;

	return transceive(&req, 1U, rx, sizeof(rx), 7U) == 2;
}

/**
 * @brief Executa a anticolisão/seleção em cascata do cartão presente, deixando-o ativo (sem
 * HLTA) para permitir autenticação e leitura/escrita de blocos em seguida.
 *
 * @param[out] uid_bytes Buffer para os bytes do UID.
 * @param[out] uid_len Tamanho, em bytes, do UID lido.
 * @param[out] sak_out Select Acknowledge do cartão.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int chip_select(uint8_t *uid_bytes, uint8_t *uid_len, uint8_t *sak_out)
{
	uint8_t cl = PICC_CL1;
	uint8_t out_uid[CONFIG_MFRC522_UID_MAX_LEN];
	uint8_t out_len = 0U;
	int ret;

	for (uint8_t cascade = 0U; cascade < 3U; cascade++) {
		uint8_t rx[5];
		uint8_t anticoll[2] = {cl, 0x20U};

		ret = transceive(anticoll, sizeof(anticoll), rx, sizeof(rx), 0U);
		if (ret < 5) {
			return (ret < 0) ? ret : -EIO;
		}

		/* Verificação do BCC. */
		if ((rx[0] ^ rx[1] ^ rx[2] ^ rx[3]) != rx[4]) {
			return -EBADMSG;
		}

		/* SELECT: [cl, NVB, uid[0..3], BCC, CRC_L, CRC_H]. */
		uint8_t buf[9];
		uint8_t crc_l, crc_h;

		buf[0] = cl;
		buf[1] = PICC_SELECT_NVB;
		memcpy(&buf[2], rx, 5U);

		ret = calc_crc(buf, 7U, &crc_l, &crc_h);
		if (ret != 0) {
			return ret;
		}
		buf[7] = crc_l;
		buf[8] = crc_h;

		uint8_t sel_rx[3];

		ret = transceive(buf, 9U, sel_rx, sizeof(sel_rx), 0U);
		if (ret < 1) {
			return (ret < 0) ? ret : -EIO;
		}
		*sak_out = sel_rx[0];

		if (rx[0] == PICC_CT) {
			/* Byte CT marca cascata; ignora e copia os 3 bytes reais do UID. */
			memcpy(&out_uid[out_len], &rx[1], 3U);
			out_len += 3U;
		} else {
			memcpy(&out_uid[out_len], rx, 4U);
			out_len += 4U;
		}

		if (!(*sak_out & 0x04U)) {
			break; /* Sem mais níveis de cascata. */
		}

		cl = (cl == PICC_CL1) ? PICC_CL2 : PICC_CL3;
	}

	memcpy(uid_bytes, out_uid, out_len);
	*uid_len = out_len;
	return 0;
}

/**
 * @brief Envia HLTA ao cartão selecionado e desliga o Crypto1, encerrando a sessão.
 *
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int chip_halt(void)
{
	uint8_t hlta[4];
	uint8_t crc_l, crc_h;
	uint8_t dummy[2];
	uint8_t status2;
	int ret;

	hlta[0] = PICC_HLTA;
	hlta[1] = 0x00U;
	ret = calc_crc(hlta, 2U, &crc_l, &crc_h);
	if (ret != 0) {
		return ret;
	}
	hlta[2] = crc_l;
	hlta[3] = crc_h;
	/* O cartão não responde a um HLTA bem-sucedido; timeout aqui é esperado. */
	transceive(hlta, sizeof(hlta), dummy, sizeof(dummy), 0U);

	ret = reg_read(REG_STATUS2, &status2);
	if (ret != 0) {
		return ret;
	}
	return reg_write(REG_STATUS2, status2 & (uint8_t)~STATUS2_CRYPTO1_ON);
}

/**
 * @brief Autentica um bloco do cartão selecionado com a Key A ou Key B do setor.
 *
 * @param auth_cmd PICC_MF_AUTH_KEY_A ou PICC_MF_AUTH_KEY_B.
 * @param block_addr Endereço absoluto do bloco.
 * @param key Chave de 6 bytes.
 * @param uid_bytes Bytes do UID retornados por @ref chip_select.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int pcd_authenticate(uint8_t auth_cmd, uint8_t block_addr, const uint8_t *key,
			    const uint8_t *uid_bytes)
{
	uint8_t buf[12];
	uint8_t irq, err;
	int ret;
	int timeout = TRANSCEIVE_TIMEOUT_MS;

	buf[0] = auth_cmd;
	buf[1] = block_addr;
	memcpy(&buf[2], key, MFRC522_KEY_SIZE);
	/* Autenticação usa os 4 últimos bytes do número de série (UID) do cartão. */
	memcpy(&buf[8], uid_bytes, 4U);

	ret = reg_write(REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_COM_IRQ, 0x7FU);
	if (ret != 0) {
		return ret;
	}
	ret = reg_write(REG_FIFO_LEVEL, 0x80U);
	if (ret != 0) {
		return ret;
	}

	for (uint8_t i = 0; i < sizeof(buf); i++) {
		ret = reg_write(REG_FIFO_DATA, buf[i]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = reg_write(REG_COMMAND, CMD_MF_AUTHENT);
	if (ret != 0) {
		return ret;
	}

	/* MFAuthent sinaliza IdleIRq(4) ao concluir, e não RxIRq. */
	do {
		ret = reg_read(REG_COM_IRQ, &irq);
		if (ret != 0) {
			return ret;
		}
		k_msleep(1);
		timeout--;
	} while (!(irq & 0x10U) && timeout > 0);

	if (timeout == 0) {
		LOG_DBG("pcd_authenticate: timed out");
		return -ETIMEDOUT;
	}

	ret = reg_read(REG_ERROR, &err);
	if (ret != 0) {
		return ret;
	}
	if (err & 0x1FU) {
		LOG_DBG("pcd_authenticate: error detected");
		return -EIO;
	}

	ret = reg_read(REG_STATUS2, &err);
	if (ret != 0) {
		return ret;
	}
	if (!(err & STATUS2_CRYPTO1_ON)) {
		LOG_DBG("pcd_authenticate: Crypto1 not engaged");
		return -EACCES;
	}

	return 0;
}

/**
 * @brief Lê um bloco de 16 bytes do cartão. Requer autenticação prévia do bloco.
 *
 * @param block_addr Endereço absoluto do bloco.
 * @param[out] data Buffer de 16 bytes para os dados lidos.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int pcd_read_block(uint8_t block_addr, uint8_t *data)
{
	uint8_t cmd[4];
	uint8_t crc_l, crc_h;
	uint8_t rx[MFRC522_BLOCK_SIZE + 2U];
	int ret;

	cmd[0] = PICC_MF_READ;
	cmd[1] = block_addr;
	ret = calc_crc(cmd, 2U, &crc_l, &crc_h);
	if (ret != 0) {
		return ret;
	}
	cmd[2] = crc_l;
	cmd[3] = crc_h;

	ret = transceive(cmd, sizeof(cmd), rx, sizeof(rx), 0U);
	if (ret != (int)sizeof(rx)) {
		LOG_DBG("pcd_read_block: transceive failed: %d", ret);
		return (ret < 0) ? ret : -EIO;
	}

	memcpy(data, rx, MFRC522_BLOCK_SIZE);
	return 0;
}

/**
 * @brief Escreve um bloco de 16 bytes no cartão. Requer autenticação prévia do bloco.
 *
 * @param block_addr Endereço absoluto do bloco.
 * @param data Buffer de 16 bytes a serem escritos.
 * @return 0 em caso de sucesso, código de erro negativo caso contrário.
 */
static int pcd_write_block(uint8_t block_addr, const uint8_t *data)
{
	uint8_t cmd[4];
	uint8_t crc_l, crc_h;
	uint8_t ack;
	uint8_t data_buf[MFRC522_BLOCK_SIZE + 2U];
	int ret;

	cmd[0] = PICC_MF_WRITE;
	cmd[1] = block_addr;
	ret = calc_crc(cmd, 2U, &crc_l, &crc_h);
	if (ret != 0) {
		return ret;
	}
	cmd[2] = crc_l;
	cmd[3] = crc_h;

	ret = transceive(cmd, sizeof(cmd), &ack, 1U, 0U);
	if (ret < 1 || (ack & 0x0FU) != PICC_MF_ACK) {
		LOG_DBG("pcd_write_block: write command not ack'd: %d", ret);
		return (ret < 0) ? ret : -EIO;
	}

	memcpy(data_buf, data, MFRC522_BLOCK_SIZE);
	ret = calc_crc(data_buf, MFRC522_BLOCK_SIZE, &crc_l, &crc_h);
	if (ret != 0) {
		return ret;
	}
	data_buf[MFRC522_BLOCK_SIZE] = crc_l;
	data_buf[MFRC522_BLOCK_SIZE + 1U] = crc_h;

	ret = transceive(data_buf, sizeof(data_buf), &ack, 1U, 0U);
	if (ret < 1 || (ack & 0x0FU) != PICC_MF_ACK) {
		LOG_DBG("pcd_write_block: data not ack'd: %d", ret);
		return (ret < 0) ? ret : -EIO;
	}

	return 0;
}

static int antenna_on(void)
{
	uint8_t val;
	int ret;

	ret = reg_read(REG_TX_CONTROL, &val);
	if (ret != 0) {
		return ret;
	}
	return reg_write(REG_TX_CONTROL, val | 0x03U);
}

bool mfrc522_init(void)
{
	int ret;
	uint8_t ver;

	self.initialized = false;

	transport_init();

	ret = reg_write(REG_COMMAND, CMD_SOFT_RESET);
	if (ret != 0) {
		return false;
	}
	k_msleep(50);

	ret = reg_write(REG_T_MODE, 0x80U); /* TAuto = 1. */
	if (ret != 0) {
		return false;
	}
	ret = reg_write(REG_T_PRESCALER, 0xA9U);
	if (ret != 0) {
		return false;
	}
	ret = reg_write(REG_T_RELOAD_H, 0x03U);
	if (ret != 0) {
		return false;
	}
	ret = reg_write(REG_T_RELOAD_L, 0xE8U);
	if (ret != 0) {
		return false;
	}
	ret = reg_write(REG_TX_ASK, 0x40U); /* 100% ASK. */
	if (ret != 0) {
		return false;
	}
	ret = reg_write(REG_MODE, 0x3DU); /* Preset do CRC 0x6363. */
	if (ret != 0) {
		return false;
	}

	ret = antenna_on();
	if (ret != 0) {
		return false;
	}

	ret = reg_read(REG_VERSION, &ver);
	if (ret != 0) {
		LOG_ERR("mfrc522_init: failed to read VersionReg: %d", ret);
		return false;
	}
	if (ver == 0x00U || ver == 0xFFU) {
		LOG_ERR("mfrc522_init: VersionReg 0x%02X — no SPI communication", ver);
		return false;
	}
	if (ver != 0x91U && ver != 0x92U) {
		LOG_WRN("mfrc522_init: clone IC detected (VersionReg=0x%02X), proceeding", ver);
	} else {
		LOG_INF("mfrc522_init: NXP IC v%u.0 detected", ver & 0x0FU);
	}

	self.initialized = true;
	LOG_INF("MFRC522 initialized");
	return true;
}

bool mfrc522_card_present(void)
{
	if (!self.initialized) {
		return false;
	}

	return chip_card_present();
}

bool mfrc522_read_uid(struct mfrc522_uid *uid)
{
	if (!mfrc522_select(uid)) {
		return false;
	}

	return chip_halt() == 0;
}

bool mfrc522_select(struct mfrc522_uid *uid)
{
	if (!self.initialized || uid == NULL) {
		return false;
	}

	return chip_select(uid->bytes, &uid->len, &uid->sak) == 0;
}

bool mfrc522_authenticate(uint8_t block_addr, enum mfrc522_key_type key_type,
			  const uint8_t key[MFRC522_KEY_SIZE], const struct mfrc522_uid *uid)
{
	uint8_t auth_cmd;

	if (!self.initialized || key == NULL || uid == NULL || uid->len < 4U) {
		return false;
	}

	auth_cmd = (key_type == MFRC522_KEY_B) ? PICC_MF_AUTH_KEY_B : PICC_MF_AUTH_KEY_A;

	/* A autenticação usa apenas os 4 últimos bytes do UID (número de série "atômico"),
	 * independentemente do UID ter 4, 7 ou 10 bytes. */
	return pcd_authenticate(auth_cmd, block_addr, key, &uid->bytes[uid->len - 4U]) == 0;
}

bool mfrc522_read_block(uint8_t block_addr, uint8_t data[MFRC522_BLOCK_SIZE])
{
	if (!self.initialized || data == NULL) {
		return false;
	}

	return pcd_read_block(block_addr, data) == 0;
}

bool mfrc522_write_block(uint8_t block_addr, const uint8_t data[MFRC522_BLOCK_SIZE])
{
	if (!self.initialized || data == NULL) {
		return false;
	}

	return pcd_write_block(block_addr, data) == 0;
}

bool mfrc522_halt(void)
{
	if (!self.initialized) {
		return false;
	}

	return chip_halt() == 0;
}
