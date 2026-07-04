/**
 * @file transport_spi.c
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @brief Define os drivers de comunicação SPI para o MFRC522.
 * @version 0.1
 * @date 04-07-2026
 *
 * @copyright Copyright (c) 2026, Centro de Inovação EDGE.
 *
 */

#include "mfrc522_transport.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

/**
 * @brief Configuração do SPI do MFRC522.
 */
#define SPI_FLAGS (SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE)

/**
 * @brief Máscara do endereço do registrador no byte de comando SPI.
 */
#define REG_ADDR_MASK 0x7EU

/**
 * @brief Bit que indica uma operação de leitura no byte de comando SPI.
 */
#define REG_READ_BIT 0x80U

/**
 * @brief Estrutura que representa o SPI do MFRC522.
 */
static const struct spi_dt_spec mfrc522_spi =
    SPI_DT_SPEC_GET(DT_NODELABEL(mfrc522_spi), SPI_FLAGS, 0);

/**
 * @brief Estrutura que representa o pino de reset do MFRC522.
 */
static const struct gpio_dt_spec mfrc522_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(mfrc522_spi), reset_gpios);

void mfrc522_transport_init(void) {
  gpio_pin_configure_dt(&mfrc522_reset, GPIO_OUTPUT_ACTIVE);

  while (!spi_is_ready_dt(&mfrc522_spi)) {
    /* Espera a comunicação SPI estar pronta. */
  }

  /* Reset físico: mantém RST em nível alto e depois libera em nível baixo. */
  gpio_pin_set_dt(&mfrc522_reset, 1);
  k_msleep(50);
  gpio_pin_set_dt(&mfrc522_reset, 0);
}

int mfrc522_transport_reg_write(uint8_t addr, uint8_t val) {
  uint8_t tx[2] = {(uint8_t)((addr << 1) & REG_ADDR_MASK), val};
  struct spi_buf tx_buf = {.buf = tx, .len = sizeof(tx)};
  struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1U};

  return spi_write_dt(&mfrc522_spi, &tx_set);
}

int mfrc522_transport_reg_read(uint8_t addr, uint8_t *val) {
  uint8_t tx[2] = {(uint8_t)(((addr << 1) & REG_ADDR_MASK) | REG_READ_BIT),
                   0x00U};
  uint8_t rx[2] = {0};
  struct spi_buf tx_buf = {.buf = tx, .len = sizeof(tx)};
  struct spi_buf rx_buf = {.buf = rx, .len = sizeof(rx)};
  struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1U};
  struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1U};
  int ret;

  ret = spi_transceive_dt(&mfrc522_spi, &tx_set, &rx_set);
  if (ret == 0) {
    *val = rx[1];
  }
  return ret;
}
