// Flash this on a second ESP32S2 to turn it into an UART bridge (Pipe the main board's uart to the PC).

void setup()
{
  // Standard CDC for Serial Monitor (ensure USB CDC On Boot is ENABLED in Tools).
  Serial.begin(115200);
  // Hardware link from target.
  Serial1.begin(115200, SERIAL_8N1, 18, 17);

  pinMode(LED_BUILTIN, OUTPUT);
}

long last_tx_rx = 0;
long led_decay = 20; // How many millis to wait before turning the light back on.
void loop()
{
  // From board to PC
  while (Serial1.available()) {
    last_tx_rx = millis();
    char buf[256];
    auto bufsize = Serial1.read(buf, 256);
    Serial.write(buf, bufsize);
  }

  // From Target to PC.
  // while (Serial.available()) {
  //   last_tx_rx = millis();
  //   Serial1.write(Serial.read());
  // }

  digitalWrite(LED_BUILTIN, millis() - last_tx_rx >= led_decay);
}
