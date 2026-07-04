/**
 * @file transport_i2c.c
 * @author João Matheus Nascimento Dias (joao.dias@edge.ufal.br)
 * @author José Félix de Oliveira Neto (josefelix.neto@edge.ufal.br)
 * @brief Define os drivers de comunicação I2C para o MFRC522.
 * @version 0.1
 * @date 04-07-2026
 *
 * @copyright Copyright (c) 2026, Centro de Inovação EDGE.
 *
 */

#include "mfrc522_transport.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

/**
 * @brief Estrutura que representa o I2C do MFRC522.
 */
static const struct i2c_dt_spec mfrc522_i2c =
    I2C_DT_SPEC_GET(DT_NODELABEL(mfrc522_i2c));

/**
 * @brief Estrutura que representa o pino de reset do MFRC522.
 */
static const struct gpio_dt_spec mfrc522_reset =
    GPIO_DT_SPEC_GET(DT_NODELABEL(mfrc522_i2c), reset_gpios);

void mfrc522_transport_init(void) {
  gpio_pin_configure_dt(&mfrc522_reset, GPIO_OUTPUT_ACTIVE);

  while (!i2c_is_ready_dt(&mfrc522_i2c)) {
    /* Espera a comunicação I2C estar pronta. */
  }

  /* Reset físico: mantém RST em nível alto e depois libera em nível baixo. */
  gpio_pin_set_dt(&mfrc522_reset, 1);
  k_msleep(50);
  gpio_pin_set_dt(&mfrc522_reset, 0);
}

int mfrc522_transport_reg_write(uint8_t addr, uint8_t val) {
  return i2c_reg_write_byte_dt(&mfrc522_i2c, addr, val);
}

int mfrc522_transport_reg_read(uint8_t addr, uint8_t *val) {
  return i2c_reg_read_byte_dt(&mfrc522_i2c, addr, val);
}
