#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include "hardware/pwm.h"

// MACアドレス（任意でOK）
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetServer server(80);  // Webサーバ
bool gpio4State = false;    // GPIO4の状態を保持
bool gpio5State = false;    // GPIO5の状態を保持
bool gpio6State = false;    // GPIO6の状態を保持
bool gpio7State = false;    // GPIO7の状態を保持
bool gpio22State = false;    // GPIO22の状態を保持

bool csState = false;
String spiSendBuffer = "", spiRecvBuffer = "";
String usbSendBuffer = "", usbRecvBuffer = "";
String i2cRecvBuffer = "";

bool PulseActive = false;
volatile int PPS;
unsigned long lastToggle = 0;
bool pulseState = false;

int pwmDuty = 50;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // I2Cマスタ設定
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.begin();

  // SPI1設定
  SPI1.setSCK(10);
  SPI1.setTX(11);
  SPI1.setRX(12);
  SPI1.begin();
  pinMode(13, OUTPUT);  // SPI1 CS
  digitalWrite(13, HIGH);

  Ethernet.init(17);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP失敗");
    while (true);
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  server.begin();

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH);

  pinMode(25, OUTPUT);
  digitalWrite(25, HIGH);

}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    usbRecvBuffer += c;
    if (usbRecvBuffer.length() > 1024)
      usbRecvBuffer = usbRecvBuffer.substring(usbRecvBuffer.length() - 1024);
  }
  EthernetClient client = server.available();
  if (client) {

      if (client.connected() && client.available()) {
        String req = client.readStringUntil('\n');
        client.read();

        Serial.print("HTTPリクエスト: ");
        Serial.println(req);  // ← デバッグ出力で確認！

        if (req.indexOf("GET /data") >=0) {
          handleDigitlData(client);
        } else if (req.indexOf("GET /toggle?pin=") >= 0) {
          handleToggleData(client, req);
        } else if (req.indexOf("GET /sendUSB?data=") >= 0) {
          handleSendUSB(client, req);
        } else if (req.indexOf("GET /recvUSB") >= 0) {
          handleRecvUSB(client);
        } else if (req.indexOf("GET /toggleCS") >= 0) {
          handleToggleCS(client);
        } else if (req.indexOf("GET /getCSState") >= 0) {
          handleGetCSState(client);
        } else if (req.indexOf("GET /sendSPI?data=") >= 0) {
          handleSendSPI(client, req);
        } else if (req.indexOf("GET /sendI2C?") >= 0) {
          handleSendI2C(client, req);
        } else if (req.indexOf("GET /togglePls") >= 0) {
          handleTogglePls(client);
        } else if (req.indexOf("GET /getPlsState") >= 0) {
          handleGetPlsState(client);
        } else if (req.indexOf("GET /setPPS") >= 0) {
          handleSetPPS(client, req);
        } else if (req.indexOf("GET /setPWM") >= 0) {
          handleSetPWM(client, req);
        } else {
          sendWebPage(client);
        }
      }

    delay(1);
    client.stop();
  }
  if (PulseActive && PPS > 0) {
    unsigned long interval = 1000UL / (PPS * 2);
    if (millis() - lastToggle >= interval) {
      pulseState = !pulseState;
      digitalWrite(22, pulseState ? HIGH : LOW);
      lastToggle = millis();
    }
  } else {
    digitalWrite(22, LOW);
  }
}

void handleDigitlData(EthernetClient& client) {
  int gpio2 = digitalRead(2);
  int gpio3 = digitalRead(3);
  int gpio4 = digitalRead(4);
  int gpio5 = digitalRead(5);
  int gpio6 = digitalRead(6);
  int gpio7 = digitalRead(7);
  int gpio22 = digitalRead(22);
  int adc0 = analogRead(26);
  int adc1 = analogRead(27);
  int adc2 = analogRead(28);
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
  client.print("{\"gpio2\":");
  client.print(gpio2);
  client.print(",\"gpio3\":");
  client.print(gpio3);
  client.print(",\"gpio4\":");
  client.print(gpio4);
  client.print(",\"gpio5\":");
  client.print(gpio5);
  client.print(",\"gpio6\":");
  client.print(gpio6);
  client.print(",\"gpio7\":");
  client.print(gpio7);
  client.print(",\"gpio22\":");
  client.print(gpio22);
  client.print(",\"adc0\":");
  client.print(adc0);
  client.print(",\"adc1\":");
  client.print(adc1);
  client.print(",\"adc2\":");
  client.print(adc2);
  client.println("}");
}

void handleToggleData(EthernetClient& client, String req) {
  String pinStr = extractParam(req, "/toggle?pin=");
  int pin = pinStr.toInt();

  if (pin == 4 || pin == 5) {
    bool state = digitalRead(pin);
    digitalWrite(pin, !state);
    sendPlain(client, "Toggled GPIO" + String(pin) + " to " + String(!state));
  } else {
    sendPlain(client, "Unsupported GPIO: " + pinStr);
  }
}

void handleSendUSB(EthernetClient& client, String req) {
  String param = extractParam(req, "GET /sendUSB?data=");
  usbSendBuffer = param;
  Serial.print(usbSendBuffer);
  sendPlain(client, "OK");
}

void handleRecvUSB(EthernetClient& client) {
  sendPlain(client, usbRecvBuffer);
  usbRecvBuffer = "";
}

void handleToggleCS(EthernetClient& client) {
  csState = !csState;
  digitalWrite(13, csState ? LOW : HIGH);
  sendPlain(client, csState ? "CS LOW (Selected)" : "CS HIGH (Deselected)");
}

void handleGetCSState(EthernetClient& client) {
  sendPlain(client, csState ? "LOW" : "HIGH");
}

void handleSendSPI(EthernetClient& client, String req) {
  String param = extractParam(req, "GET /sendSPI?data=");
  spiSendBuffer = param;
  spiRecvBuffer = spiTransfer(spiSendBuffer);
  sendPlain(client, spiRecvBuffer);
}

void handleSendI2C(EthernetClient& client, String req) {
  // 例: GET /sendI2C?addr=0x40&write=xx OR &read=5
  int addrIndex = req.indexOf("addr=");
  if (addrIndex < 0) {
    sendPlain(client, "Missing addr param");
    return;
  }
  int addrEnd = req.indexOf('&', addrIndex);
  String addrStr = req.substring(addrIndex + 5, addrEnd);
  int addr = strtol(addrStr.c_str(), NULL, 0);

  if (req.indexOf("write=") >= 0) {
    String data = extractParam(req, "write=");
    Wire.beginTransmission(addr);
    for (int i = 0; i < data.length(); i++) {
      Wire.write((uint8_t)data[i]);
    }
    byte result = Wire.endTransmission();
    sendPlain(client, result == 0 ? "Write OK" : "Write Failed: " + String(result));
  } else if (req.indexOf("read=") >= 0) {
    String numStr = extractParam(req, "read=");
    int num = numStr.toInt();
    Wire.requestFrom(addr, num);
    String result = "";
    while (Wire.available()) {
      result += (char)Wire.read();
    }
    sendPlain(client, result.length() ? result : "No data");
  } else {
    sendPlain(client, "No write/read param");
  }
}

String extractParam(String req, const char* key) {
  int idx = req.indexOf(key);
  if (idx < 0) return "";
  String param = req.substring(idx + strlen(key));
  int endIdx = param.indexOf(' ');
  if (endIdx >= 0) param = param.substring(0, endIdx);
  param.replace("%20", " ");
  return param;
}

String spiTransfer(const String& sendStr) {
  String recvStr = "";
  digitalWrite(13, LOW);
  delay(1);
  for (int i = 0; i < sendStr.length(); i++) {
    uint8_t recvByte = SPI1.transfer((uint8_t)sendStr[i]);
    recvStr += (char)recvByte;
  }
  digitalWrite(13, HIGH);
  return recvStr;
}

void sendPlain(EthernetClient& client, const String& message) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain; charset=utf-8");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
  client.println(message);
}

void handleTogglePls(EthernetClient& client) {
  PulseActive = !PulseActive;
  sendPlain(client, PulseActive ? "PulseActive HIGH (Selected)" : "PulseActive LOW (Deselected)");
}

void handleGetPlsState(EthernetClient& client) {
  sendPlain(client, PulseActive ? "HIGH" : "LOW");
}

void handleSetPPS(EthernetClient& client, String req) {
  int idx = req.indexOf("value=");
  if (idx >= 0) {
    String valStr = req.substring(idx + 6); 
    int amp = valStr.indexOf('&'); 
    if (amp >= 0) valStr = valStr.substring(0, amp);
    PPS = valStr.toInt(); 
  }
}

void handleSetPWM(EthernetClient& client, String req) {
  int idx = req.indexOf("duty=");
  if (idx >= 0) {
    String valStr = req.substring(idx + 5);
    int amp = valStr.indexOf('&');
    if (amp >= 0) valStr = valStr.substring(0, amp);
    int val = valStr.toInt();
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    pwmDuty = val;
    setPWMDuty(pwmDuty);
    sendPlain(client, "OK");
  } else {
    sendPlain(client, "dutyパラメータがありません");
  }
}

void setPWMDuty(int duty) {
  int gpio = 25;
  int freq = 1000;
  gpio_set_function(gpio, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(gpio);
  int clock = 125000000;
  int top = clock / freq;
  pwm_set_wrap(slice_num, top);
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(gpio), top * duty / 100);
  pwm_set_enabled(slice_num, true);
}

void sendWebPage(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>W5100S IOモニタ</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: sans-serif; padding: 20px; }
    #gpioState { white-space: pre-wrap; background: #eee; padding: 10px; border: 1px solid #ccc; font-size: 16px; width: 900px; }
    #adcChart1 { width: 500px !important; height: auto !important; }
    #adcChart2 { width: 500px !important; height: auto !important; }
    #adcChart3 { width: 500px !important; height: auto !important; }
    #digitalChart1 { width: 500px !important; height: auto !important; }
    #digitalChart2 { width: 500px !important; height: auto !important; }
    #digitalChart3 { width: 500px !important; height: auto !important; }
    #usbRecv, #spiRecv, #i2cRecv {
      white-space: pre-wrap; background:#eef; padding:10px;
      height:100px; overflow-y:auto;
    }
    .container {
      display: flex;
      gap: 20px;
      justify-content: flex-start;
      align-items: flex-start;
      flex-wrap: wrap; /* 小画面で折り返し対応したいなら追加 */
    }
    .section {
      border: 1px solid #ccc;
      padding: 10px;
      width: 900px;
      background: #f8f8f8;
    }
    textarea {
      width: 100%;
      height: 100px;
      resize: vertical;
    }
    input[type="text"] {
    width: 80%;
    }
    button {
      width: 18%;
    }
  </style>
</head>
  )rawliteral");

  client.println(R"rawliteral(
<body>
  <h2>GPIO & アナログモニタ</h2>
  <p id="gpioState">取得中...</p>
  <button onclick="toggleGPIO(4)" style="width: 120px;">GPIO4 トグル</button>
  <button onclick="toggleGPIO(5)" style="width: 120px;">GPIO5 トグル</button>
  <div style="display: flex; gap: 20px;">
    <canvas id="adcChart1" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
    <canvas id="adcChart2" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
    <canvas id="adcChart3" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
    <canvas id="digitalChart1" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
    <canvas id="digitalChart2" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
    <canvas id="digitalChart3" width="400" height="200" style="width: 300px; height: 150px;"></canvas>
  </div>
  )rawliteral");

  client.println(R"rawliteral(
  <h2>通信モニタ</h2>
  <div class="container">
    <div class="section">
      <h3>USBシリアル</h3>
      <input type="text" id="usbSend" placeholder="USBシリアル送信" />
      <button onclick="sendUSB()">送信</button>
      <div id="usbRecv" style="min-height: 300px; border: 1px solid #ccc; padding: 5px;"></div>
    </div>
    <div class="section">
      <div style="display: flex; align-items: center; gap: 10px;">
        <span style="font-size: 1.17em; font-weight: bold;">SPI1</span>
        <p>CS (GPIO13) 状態: <span id="csState">取得中...</span>
        <button onclick="toggleCS()" style="width: 120px;">トグルCS</button>
        </p>
      </div>
      <div style="margin-top: 10px;">
        <input type="text" id="spiSend" placeholder="SPI送信文字列" />
        <button onclick="sendSPI()">送信</button>
        <div id="spiRecv" style="min-height: 300px; border: 1px solid #ccc; padding: 5px;"></div>
      </div>
    </div>
    <div class="section">
      <div style="display: flex; align-items: center; gap: 10px; white-space: nowrap; margin-top: 10px;">
        <span style="font-size: 1.17em; font-weight: bold;">I2C</span>
        <span>アドレス: <input type="text" id="i2cAddr" placeholder="例: 0x40" style="width: 80px;" /></span>
        <span>読み取りバイト数: <input type="text" id="i2cReadLen" placeholder="例: 5" style="width: 80px;" /></span>
        <button onclick="sendI2CRead()">読取</button>
      </div>
      <div style="margin-top: 30px;">
        <span>書き込みデータ: <input type="text" id="i2cWrite" placeholder="送信文字列" style="width: 600px;" /></span>
        <button onclick="sendI2CWrite()">送信</button>
        <div id="i2cRecv" style="min-height: 300px; border: 1px solid #ccc; padding: 5px;"></div>
      </div>
    </div>
  </div>
  )rawliteral");

  client.println(R"rawliteral(
  <h2>パルスジェネレータ</h2>
    <input type="number" id="ppsInput" value="1" min="0" max="1000"> PPS
    <button onclick="setPPS()" style="width: 120px;">PPS変更</button>
    <span>CS (GPIO22) 状態: <span id="PulseActive">取得中...</span>
    <button onclick="togglePls()" style="width: 200px;">GPIO22 パルス出力トグル</button>
  <h2>PWMジェネレータ</h2>
    <label for="pwmSlider">PWMデューティ (%)</label>
    <input type="range" id="pwmSlider" min="0" max="100" value="50" />
    <input type="number" id="pwmValue" min="0" max="100" value="50" style="width: 50px;" />
    <button onclick="sendPWM()" style="width: 120px;">設定送信</button>
  )rawliteral");

  client.println(R"rawliteral(
  <script>
    function toggleGPIO(pin) {
      fetch(`/toggle?pin=${pin}`)
        .then(res => res.text())
        .then(text => {
          alert(text);
        });
    }
    const ctx1 = document.getElementById('adcChart1').getContext('2d');
    const adcData1 = {
      labels: [],
      datasets: [
        {label: 'ADC0', data: [], borderColor: 'blue', fill: false}
      ]
    };
    const chart1 = new Chart(ctx1, {type: 'line', data: adcData1, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: 0, max: 4095 } } } });
    const ctx2 = document.getElementById('adcChart2').getContext('2d');
    const adcData2 = {
      labels: [],
      datasets: [
        {label: 'ADC1', data: [], borderColor: 'red', fill: false}
      ]
    };
    const chart2 = new Chart(ctx2, {type: 'line', data: adcData2, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: 0, max: 4095 } } } });
  )rawliteral");

  client.println(R"rawliteral(
    const ctx3 = document.getElementById('adcChart3').getContext('2d');
    const adcData3 = {
      labels: [],
      datasets: [
        {label: 'ADC2', data: [], borderColor: 'green', fill: false}
      ]
    };
    const chart3 = new Chart(ctx3, {type: 'line', data: adcData3, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: 0, max: 4095 } } } });
    const ctx4 = document.getElementById('digitalChart1').getContext('2d');
    const digitalData1 = {
      labels: [],
      datasets: [
        {label: 'GPIO2', data: [], borderColor: 'orange', borderWidth: 2, fill: false, stepped: true}
      ]
    };
   const chart4 = new Chart(ctx4, {type: 'line', data: digitalData1, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: -0.2, max: 1.2, ticks: { stepSize: 1, callback: value => value === 1 ? 'ON' : 'OFF' } } } } });
    const ctx5 = document.getElementById('digitalChart2').getContext('2d');
    const digitalData2 = {
      labels: [],
      datasets: [
        {label: 'GPIO3', data: [], borderColor: 'brown', borderWidth: 2, fill: false, stepped: true}
      ]
    };
    const chart5 = new Chart(ctx5, {type: 'line', data: digitalData2, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: -0.2, max: 1.2, ticks: { stepSize: 1, callback: value => value === 1 ? 'ON' : 'OFF' } } } } });
  )rawliteral");

  client.println(R"rawliteral(
    const ctx6 = document.getElementById('digitalChart3').getContext('2d');
    const digitalData3 = {
      labels: [],
      datasets: [
        {label: 'GPIO4', data: [], borderColor: 'black', borderWidth: 2, fill: false, stepped: true}
      ]
    };
    const chart6 = new Chart(ctx6, {type: 'line', data: digitalData3, options: {responsive: true, maintainAspectRatio: true, animation: false, scales: { x: { title: { display: true, text: '時刻' } }, y: { min: -0.2, max: 1.2, ticks: { stepSize: 1, callback: value => value === 1 ? 'ON' : 'OFF' } } } } });
    function pollUSBRecv() {
      fetch('/recvUSB').then(r => r.text()).then(text => {
        if (text) {
          const now = new Date().toLocaleTimeString();
          document.getElementById('usbRecv').textContent += `${now} 受信: ` + text.replace(/\n+$/, '') + '\n';
          document.getElementById('usbRecv').scrollTop = document.getElementById('usbRecv').scrollHeight;
        }
      });
    }
    function sendUSB() {
      const data = document.getElementById('usbSend').value;
      fetch('/sendUSB?data=' + encodeURIComponent(data))
        .then(r => r.text()).then(() => {
          document.getElementById('usbRecv').textContent += '送信: ' + data + '\n';
        });
    }
    function getCSState() {
      fetch('/getCSState').then(r => r.text()).then(text => {
        document.getElementById('csState').textContent = text;
      });
    }
    function toggleCS() {
      fetch('/toggleCS').then(r => r.text()).then(text => {
        document.getElementById('csState').textContent = text;
      });
    }
    function sendSPI() {
      const data = document.getElementById('spiSend').value;
      fetch('/sendSPI?data=' + encodeURIComponent(data))
        .then(r => r.text()).then(text => {
          document.getElementById('spiRecv').textContent += '受信: ' + text + '\\n';
          document.getElementById('spiRecv').scrollTop = document.getElementById('spiRecv').scrollHeight;
        });
    }
  )rawliteral");

  client.println(R"rawliteral(
  function getPlsState() {
    fetch('/getPlsState').then(r => r.text()).then(text => {
       document.getElementById('PulseActive').textContent = text;
    });
  }
  )rawliteral");

  client.println(R"rawliteral(
  function togglePls() {
    fetch('/togglePls').then(r => r.text()).then(text => {
      document.getElementById('PulseActive').textContent = text;
    });
  }
  )rawliteral");

  client.println(R"rawliteral(
  function setPPS() {
    const val = document.getElementById('ppsInput').value;
    fetch('/setPPS?value=' + encodeURIComponent(val)).then(r => r.text()).then(text => {
      document.getElementById('ppsInput').textContent = text;
    });
  }
  )rawliteral");

  client.println(R"rawliteral(
  function sendI2CWrite() {
    const addr = document.getElementById('i2cAddr').value;
    const data = document.getElementById('i2cWrite').value;
    fetch("/sendI2C?addr=" + encodeURIComponent(addr) + "&write=" + encodeURIComponent(data))
      .then(r => r.text()).then(text => {
        document.getElementById('i2cRecv').textContent += '書き込み: ' + text + '\n';
      });
  }
  )rawliteral");

  client.println(R"rawliteral(
  function sendI2CRead() {
    const addr = document.getElementById('i2cAddr').value;
    const len = document.getElementById('i2cReadLen').value;
    fetch("/sendI2C?addr=" + encodeURIComponent(addr) + "&read=" + encodeURIComponent(len))
      .then(r => r.text()).then(text => {
        document.getElementById('i2cRecv').textContent += '読取り: ' + text + '\n';
        document.getElementById('i2cRecv').scrollTop = document.getElementById('i2cRecv').scrollHeight;
      });
  }
  const slider = document.getElementById('pwmSlider');
  const numberBox = document.getElementById('pwmValue');
  slider.addEventListener('input', () => {
    numberBox.value = slider.value;
  });
  numberBox.addEventListener('input', () => {
    let val = parseInt(numberBox.value);
    if (isNaN(val)) val = 0;
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    slider.value = val;
    numberBox.value = val;
  });
  )rawliteral");

  client.println(R"rawliteral(
  function sendPWM() {
    const val = slider.value;
    fetch(`/setPWM?duty=${val}`)
      .then(response => response.text())
      .then(text => {
        alert('PWMデューティを ' + val + '% に設定しました。\nサーバー応答: ' + text);
      });
  }
  )rawliteral");

  client.println(R"rawliteral(
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          const now = new Date().toLocaleTimeString();
          document.getElementById('gpioState').textContent =
            `取得時間: ${now}, GPIO2: ${data.gpio2}, GPIO3: ${data.gpio3}, GPIO4: ${data.gpio4}, GPIO5: ${data.gpio5}, GPIO6: ${data.gpio6}, GPIO7: ${data.gpio7}, GPIO22: ${data.gpio22}, ADC0: ${data.adc0}, ADC1: ${data.adc1}, ADC2: ${data.adc2}`;
          adcData1.labels.push(now);
          adcData1.datasets[0].data.push(data.adc0);
          if (adcData1.labels.length > 20) {
            adcData1.labels.shift();
            adcData1.datasets[0].data.shift();
          }
          chart1.update();
          adcData2.labels.push(now);
          adcData2.datasets[0].data.push(data.adc1);
          if (adcData2.labels.length > 20) {
            adcData2.labels.shift();
            adcData2.datasets[0].data.shift();
          }
          chart2.update();
          adcData3.labels.push(now);
          adcData3.datasets[0].data.push(data.adc2);
          if (adcData3.labels.length > 20) {
            adcData3.labels.shift();
            adcData3.datasets[0].data.shift();
          }
          chart3.update();
  )rawliteral");

  client.println(R"rawliteral(
          digitalData1.labels.push(now);
          digitalData1.datasets[0].data.push(data.gpio2);
          if (digitalData1.labels.length > 20) {
            digitalData1.labels.shift();
            digitalData1.datasets[0].data.shift();
          }
          chart4.update();
          digitalData2.labels.push(now);
          digitalData2.datasets[0].data.push(data.gpio3);
          if (digitalData2.labels.length > 20) {
            digitalData2.labels.shift();
            digitalData2.datasets[0].data.shift();
          }
          chart5.update();
          digitalData3.labels.push(now);
          digitalData3.datasets[0].data.push(data.gpio4);
          if (digitalData3.labels.length > 20) {
            digitalData3.labels.shift();
            digitalData3.datasets[0].data.shift();
          }
          chart6.update();
          pollUSBRecv();
          getCSState();
          getPlsState();
        })
    }
    setInterval(updateData, 3000);
    updateData();
  </script>
</body>
</html>
  )rawliteral");
}