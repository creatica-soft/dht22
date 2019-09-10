//DHT22 temperature and humidity sensor that uses pulse length data encoding over single line
//Power Supply 5V
#define data_pin 22
#define median 50 //us high voltage pulse length: short pulse for "0" is 26-28us, long pulse for "1" is 70us; low voltage pulse is 50us - used as a divider
#define ack_response_time 80 //us
#define deviation 20 //us

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println(F("ready"));
}

void loop() {
  uint8_t deg[3] = { 0xc2, 0xb0 }; //unicode degree symbol
  uint16_t th = 0, tl = 0, hh = 0, hl = 0, cs = 0;
  uint32_t data[40], response;
  char s[128];

  //close the bus
  pinMode(data_pin, INPUT_PULLUP);
  delay(5000); //pull data every 5s

  //request data
  pinMode(data_pin, OUTPUT);
  digitalWrite(data_pin, LOW);
  delayMicroseconds(1500); //must be at least 1000us
  pinMode(data_pin, INPUT_PULLUP);

  //read ack
  response = pulseIn(data_pin, LOW);
  
  for (int i = 0; i < 40; i++) {
    data[i] = pulseIn(data_pin, HIGH);
  }

  //process data

  if (response == 0 || response < ack_response_time - deviation || response > ack_response_time + deviation) Serial.println(F("no response: missing 80us low pulse!"));
  //sprintf(s, String(F("response pulse length: low %lu us")).c_str(), response);
  //Serial.println(s);
  
  for (int i = 0; i < 8; i++) {
    //sprintf(s, String(F("%u pulse length: %lu us")).c_str(), i, data[i]);
    //Serial.println(s);
    hh <<= 1;
    if (data[i] > median) hh |= 1;
    else if (data[i] == 0) error(i);
  }
  for (int i = 8; i < 16; i++) {
    //sprintf(s, String(F("%u pulse length: %lu us")).c_str(), i, data[i]);
    //Serial.println(s);
    hl <<= 1;
    if (data[i] > median) hl |= 1;
    else if (data[i] == 0) error(i);
  }

  for (int i = 16; i < 24; i++) {
    //sprintf(s, String(F("%u pulse length: %lu us")).c_str(), i, data[i]);
    //Serial.println(s);
    th <<= 1;
    if (data[i] > median) th |= 1;
    else if (data[i] == 0) error(i);
  }

  for (int i = 24; i < 32; i++) {
    //sprintf(s, String(F("%u pulse length: %lu us")).c_str(), i, data[i]);
    //Serial.println(s);
    tl <<= 1;
    if (data[i] > median) tl |= 1;
    else if (data[i] == 0) error(i);
  }

  for (int i = 32; i < 40; i++) {
    //sprintf(s, String(F("%u pulse length: %lu us")).c_str(), i, data[i]);
    //Serial.println(s);
    cs <<= 1;
    if (data[i] > median) cs |= 1;
    else if (data[i] == 0) error(i);
  }

  if (((hh + hl + th + tl) & 0xff) != cs) {
    sprintf(s, String(F("Checksum error: hh = %u, hl = %u, th = %u, tl = %u, cs = %u")).c_str(), hh, hl, th, tl, cs);
    Serial.println(s); 
  } else {
    sprintf(s, String(F("T = %.1f%.2sC, H = %.1f%%")).c_str(), (int16_t)((th << 8) | tl) * 0.1, (char *)(&deg), ((hh << 8) | hl) * 0.1);
    Serial.println(s);
  }
}

void error(int i) {
    char s[32];
    sprintf(s, String(F("error: missing data bit %u")).c_str(), i);
    Serial.println(s);
}
