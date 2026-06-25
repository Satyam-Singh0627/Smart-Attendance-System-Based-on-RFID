RFID RC522 → ESP8266 NodeMCU
| RC522 Pin | NodeMCU Pin |
| --------- | ----------- |
| SDA (SS)  | D4 (GPIO2)  |
| SCK       | D5 (GPIO14) |
| MOSI      | D7 (GPIO13) |
| MISO      | D6 (GPIO12) |
| RST       | D3 (GPIO0)  |
| 3.3V      | 3.3V        |
| GND       | GND         |

I2C LCD (16×2) → ESP8266 NodeMCU
| LCD Pin | NodeMCU Pin |
| ------- | ----------- |
| SDA     | D2 (GPIO4)  |
| SCL     | D1 (GPIO5)  |
| VCC     | VIN (5V)    |
| GND     | GND         |

Buzzer → ESP8266 NodeMCU
| Buzzer Pin   | NodeMCU Pin |
| ------------ | ----------- |
| Positive (+) | D8 (GPIO15) |
| Negative (-) | GND         |

Power Connections
| Component         | Power Source |
| ----------------- | ------------ |
| NodeMCU           | USB 5V       |
| RC522 RFID Reader | 3.3V         |
| I2C LCD           | VIN (5V)     |
| Buzzer            | D8 + GND     |

NodeMCU Pin Usage Summary
| NodeMCU Pin | Connected To   |
| ----------- | -------------- |
| D1          | LCD SCL        |
| D2          | LCD SDA        |
| D3          | RC522 RST      |
| D4          | RC522 SDA (SS) |
| D5          | RC522 SCK      |
| D6          | RC522 MISO     |
| D7          | RC522 MOSI     |
| D8          | Buzzer         |
