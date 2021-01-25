#include "config.h"
#include <Arduino.h>
#include <PubSubClient.h>
#include <TinyGsmClient.h>
#include <SPI.h>
#include <MFRC522.h>

MFRC522 mfrc522;

// 設定有效 RFID Card UID
const uint8_t valid_cards[NUM_OF_CARD][4] = {
    // card 1
    {0xF2, 0xB8, 0x15, 0x5F},
    // card 2
    {0x34, 0x25, 0x71, 0xA7},
    // my phone(NFC)
    {0x30, 0xA8, 0x11, 0x10}
};

#ifdef DEBUG_DUMP_AT_COMMAND
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger, AM7020_RESET);
#else
// 建立 AM7020 modem（設定 Serial 及 EN Pin）
TinyGsm modem(SerialAT, AM7020_RESET);
#endif

// 在 modem 架構上建立 Tcp Client
TinyGsmClient tcpClient(modem);
// 在 Tcp Client 架構上建立 MQTT Client
PubSubClient mqttClient(MQTT_BROKER, MQTT_PORT, tcpClient);

void mqttConnect(void);
void nbConnect(void);
String UIDToHexString(uint8_t *buffer);
bool chkCardUID(uint8_t *UID);
void lock(void);
void unlock(void);

void setup()
{
    SerialMon.begin(MONITOR_BAUDRATE);
    SerialAT.begin(AM7020_BAUDRATE);

    SPI.begin();

    // 初始化 mfrc522
    mfrc522.PCD_Init(SS_PIN, RST_PIN);
    // 顯示 mfrc522 firmware 版本
    SerialMon.print(F("Reader "));
    SerialMon.print(F(": "));
    mfrc522.PCD_DumpVersionToSerial();

    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);

    // AM7020 NBIOT 連線基地台
    nbConnect();
    // 設定 MQTT KeepAlive 為270秒
    mqttClient.setKeepAlive(270);
    lock();
}

void loop()
{
    // 檢查 MQTT Client 連線狀態
    if (!mqttClient.connected()) {
        // 檢查 NBIOT 連線狀態
        if (!modem.isNetworkConnected()) {
            nbConnect();
        }
        SerialMon.println(F("=== MQTT NOT CONNECTED ==="));
        mqttConnect();
    }

    // read card
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        // get card uid
        uint8_t *uid = mfrc522.uid.uidByte;
        if (chkCardUID(uid)) {
            // send success message to adafruit io
            mqttClient.publish(MQTT_TOPIC_MSG, "歡迎回家 ！");
            SerialMon.println("Valid card");
            // unlock
            unlock();
            delay(5000);
        } else {
            // send fail message to adafruit io
            String msg = "解鎖失敗 ！ UID: " + UIDToHexString(uid);
            mqttClient.publish(MQTT_TOPIC_MSG, msg.c_str());
            SerialMon.println("Invalid card");
        }
        // lock
        lock();
        // mfrc522 stop
        mfrc522.PICC_HaltA();
    }

    // mqtt handle
    mqttClient.loop();
}

/**
 * MQTT Client 連線
 */
void mqttConnect(void)
{
    SerialMon.print(F("Connecting to "));
    SerialMon.print(MQTT_BROKER);
    SerialMon.print(F("..."));

    // Connect to MQTT Broker
    while (!mqttClient.connect("AM7020_MQTTID_RFID_20210125", MQTT_USERNAME, MQTT_PASSWORD)) {
        SerialMon.println(F(" fail"));
    }
    SerialMon.println(F(" success"));
}

/**
 * AM7020 NBIOT 連線基地台
 */
void nbConnect(void)
{
    SerialMon.println(F("Initializing modem..."));
    while (!modem.init() || !modem.nbiotConnect(APN, BAND)) {
        SerialMon.print(F("."));
    };

    SerialMon.print(F("Waiting for network..."));
    while (!modem.waitForNetwork()) {
        SerialMon.print(F("."));
    }
    SerialMon.println(F(" success"));
}

/**
 * 轉換 UID 到 hex String
 * @param UID UID 指標
 * @return UID(hex String)
 */
String UIDToHexString(uint8_t *UID)
{
    String temp = "";
    for (uint8_t i = 0; i < 4; i++) {
        temp += UID[i] < 0x10 ? " 0" : " ";
        temp += String(UID[i], HEX);
    }
    return temp;
}

/**
 * 檢查 UID 是否在有效清單內
 * @param UID UID 指標
 * @return 是否有效
 */
bool chkCardUID(uint8_t *UID) 
{
    for (int ii = 0; ii < NUM_OF_CARD; ii++) {
        int jj;
        for (jj = 0; jj < 4; jj++) {
            if (*(UID + jj) != valid_cards[ii][jj]) {
                break;
            }
        }
        if (jj == 4) {
            return true;
        }
    }
    return false;
}

/**
 * 鎖定門鎖
 */
void lock(void)
{
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    // TODO: 鎖定門鎖
}

/**
 * 開啟門鎖
 */
void unlock(void)
{
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
    // TODO: 開啟門鎖
}