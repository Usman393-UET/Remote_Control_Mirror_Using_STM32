# Remote_Control_Mirror_Using_STM32
STM32-based wireless remote control mirror system utilizing dual microcontrollers and HC-12 RF transceiver modules. The remote features a 5-button interface (Power + 4-Way Directional Control), while the receiver interprets signals to drive two servos for precise X and Y-axis panning. Includes full firmware and pin configurations.

# Wireless Remote Control Mirror System (STM32F103C8T6)

A dual-microcontroller wireless system designed to adjust a motorized mirror assembly remotely. The project utilizes two STM32 boards communicating over an RF link via HC-12 transceivers to translate button matrices into dual-axis servo panning movements.

---

## ⚡ Key System Features

### 🎮 Remote Controller (Transmitter)
* **Power Management:** Dedicated 5th button initializes or shuts down transmitter polling.
* **Navigation Inputs:** 4 directional tactile buttons mapping Up, Down, Left, and Right.
* **RF Modulation:** Broadcasts serialized data strings reliably via an HC-12 module.

### 📡 Mirror Base (Receiver)
* **Dual-Axis Actuation:** 2 independent PWM channels controlling specialized servo positions.
* **X-Axis Tracking:** Servo 1 handles structural horizontal panning.
* **Y-Axis Tracking:** Servo 2 handles structural vertical tilting.
* **Command Decoding:** Decodes wireless serial streams into degree targets instantly.

---

## 🛠️ Hardware Component Matrix

### Transmitter Node
* **MCU:** STM32 (e.g., Blue Pill / Nucleo)
* **Wireless:** HC-12 RF Transceiver
* **Inputs:** 5x Tactile Push-Buttons

### Receiver Node
* **MCU:** STM32F103C8T6
* **Wireless:** HC-12 RF Transceiver
* **Actuators:** MG996R 360 Servos
* **Power:** External 5V DC Supply (for Servos)

---

## 📌 Pin Configuration Guide

### Remote Module (Transmitter)

| Component | STM32 Interface Pin | Description |
| :--- | :--- | :--- |
| **HC-12 TX** | UART RX Pin | Serial Data Receive |
| **HC-12 RX** | UART TX Pin | Serial Data Transmit |
| **Button 1** | GPIO Input (Pull-Up) | Up Command |
| **Button 2** | GPIO Input (Pull-Up) | Down Command |
| **Button 3** | GPIO Input (Pull-Up) | Left Command |
| **Button 4** | GPIO Input (Pull-Up) | Right Command |
| **Button 5** | GPIO Input (Pull-Up) | Power Toggle |

### Mirror Module (Receiver)

| Component | STM32 Interface Pin | Description |
| :--- | :--- | :--- |
| **HC-12 TX** | UART RX Pin | Serial Data Receive |
| **HC-12 RX** | UART TX Pin | Serial Data Transmit |
| **Servo X** | Timer PWM Output | Horizontal Control |
| **Servo Y** | Timer PWM Output | Vertical Control |

---
## Authors --
Muhammad Wasim, Muhammad Usman
