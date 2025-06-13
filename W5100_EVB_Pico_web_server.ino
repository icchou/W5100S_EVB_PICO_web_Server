#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>

// MACアドレス（任意でOK）
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

EthernetServer server(80);  // Webサーバ
bool gpio4State = false;    // GPIO4の状態を保持

void setup() {
  Wire.setSDA(8);
  Wire.setSCL(9);
  SPI1.setSCK(10);
  SPI1.setTX(11);
  SPI1.setRX(12);

  Serial.begin(115200);
  while (!Serial);
  Wire.begin();
  SPI1.begin();

  Ethernet.init(17);  // W5100S-EVB-PicoのCSピン
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP失敗");
    while (true);
  }

  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  server.begin();

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        String req = client.readStringUntil('\r');
        client.read(); // '\n'

        if (req.indexOf("GET /data") >= 0) {
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
          break;

        } else if (req.indexOf("GET /toggle") >= 0) {
          gpio4State = !gpio4State;
          digitalWrite(4, gpio4State ? HIGH : LOW);

          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println("Access-Control-Allow-Origin: *");
          client.println("Connection: close");
          client.println();
          client.println(gpio4State ? "ON" : "OFF");
          break;

        } else {
          // HTMLページ送信（分割で出力）
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<!DOCTYPE html>");
          client.println("<html>");
          client.println("<head>");
          client.println("<meta charset=\"UTF-8\">");
          client.println("<title>W5100S IOモニタ</title>");
          client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>");
          client.println("</head>");
          client.println("<body>");
          client.println("<h2>GPIO & アナログモニタ</h2>");
          client.println("<p id=\"gpioState\">取得中...</p>");
          client.println("<button onclick=\"toggleGPIO4()\">GPIO4 トグル</button>");
          client.println("<canvas id=\"adcChart\" width=\"400\" height=\"200\"></canvas>");
          client.println("<script>");
          client.println("const ctx = document.getElementById('adcChart').getContext('2d');");
          client.println("const adcData = {");
          client.println("  labels: [],");
          client.println("  datasets: [");
          client.println("    { label: 'ADC0', data: [], borderColor: 'blue', fill: false },");
          client.println("    { label: 'ADC1', data: [], borderColor: 'red', fill: false },");
          client.println("    { label: 'ADC2', data: [], borderColor: 'green', fill: false }");
          client.println("  ]");
          client.println("};");
          client.println("const chart = new Chart(ctx, {");
          client.println("  type: 'line',");
          client.println("  data: adcData,");
          client.println("  options: {");
          client.println("    responsive: true,");
          client.println("    animation: false,");
          client.println("    scales: {");
          client.println("      x: { title: { display: true, text: '時刻' } },");
          client.println("      y: { min: 0, max: 4095 }");
          client.println("    }");
          client.println("  }");
          client.println("});");

          client.println("function updateData() {");
          client.println("  fetch('/data')");
          client.println("    .then(response => response.json())");
          client.println("    .then(data => {");
          client.println("      const now = new Date().toLocaleTimeString();");
          client.println("      document.getElementById('gpioState').textContent =");
          client.println("        `GPIO2: ${data.gpio2}, GPIO3: ${data.gpio3}, GPIO4: ${data.gpio4}, GPIO5: ${data.gpio5}, GPIO6: ${data.gpio6}, GPIO7: ${data.gpio7}, GPIO22: ${data.gpio22}, ADC0: ${data.adc0}, ADC1: ${data.adc1}, ADC2: ${data.adc2}`;");
          client.println("      adcData.labels.push(now);");
          client.println("      adcData.datasets[0].data.push(data.adc0);");
          client.println("      adcData.datasets[1].data.push(data.adc1);");
          client.println("      adcData.datasets[2].data.push(data.adc2);");
          client.println("      if (adcData.labels.length > 20) {");
          client.println("        adcData.labels.shift();");
          client.println("        adcData.datasets.forEach(ds => ds.data.shift());");
          client.println("      }");
          client.println("      chart.update();");
          client.println("    });");
          client.println("}");

          client.println("function toggleGPIO4() {");
          client.println("  fetch('/toggle')");
          client.println("    .then(res => res.text())");
          client.println("    .then(state => {");
          client.println("      alert('GPIO4 is now: ' + state);");
          client.println("      updateData();");
          client.println("    });");
          client.println("}");

          client.println("setInterval(updateData, 3000);");
          client.println("updateData();");
          client.println("</script>");
          client.println("</body>");
          client.println("</html>");
          break;
        }
      }
    }
    delay(1);
    client.stop();
  }
}
